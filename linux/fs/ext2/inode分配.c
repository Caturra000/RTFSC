// 文件：/fs/ext2/ialloc.c

// dir：父目录对应inode
struct inode *ext2_new_inode(struct inode *dir, umode_t mode,
			     const struct qstr *qstr)
{
	struct super_block *sb;
	struct buffer_head *bitmap_bh = NULL;
	struct buffer_head *bh2;
	int group, i;
	ino_t ino = 0;
	struct inode * inode;
	struct ext2_group_desc *gdp;
	struct ext2_super_block *es;
	struct ext2_inode_info *ei;
	struct ext2_sb_info *sbi;
	int err;

	sb = dir->i_sb;
	// 分配vfs inode
	// 见fs/vfs/inode分配.c
	// 该流程需要提供一个sb->s_op->alloc_inode(sb)
	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	// 这种形式都是秀container_of操作，通过vfs inode反推出ext2_inode_info
	// 因为vfs inode就是存放在ext2_inode_info容器下
	// 其它同理
	ei = EXT2_I(inode);
	sbi = EXT2_SB(sb);
	es = sbi->s_es;
	// 这个if-else用于寻找合适的group
	// find_group流程见group查找.c
	// TODO
	if (S_ISDIR(mode)) {
		// OLD ALLOC，这是一种比较老的方式
		if (test_opt(sb, OLDALLOC))
			group = find_group_dir(sb, dir);
		else
			group = find_group_orlov(sb, dir);
	} else 
		group = find_group_other(sb, dir);

	if (group == -1) {
		err = -ENOSPC;
		goto fail;
	}

	for (i = 0; i < sbi->s_groups_count; i++) {
		gdp = ext2_get_group_desc(sb, group, &bh2);
		if (!gdp) {
			if (++group == sbi->s_groups_count)
				group = 0;
			continue;
		}
		brelse(bitmap_bh);
		// 得到所选块组的inode bitmap
		bitmap_bh = read_inode_bitmap(sb, group);
		if (!bitmap_bh) {
			err = -EIO;
			goto fail;
		}
		ino = 0;

repeat_in_this_group:
		// 在bitmap中查找空闲位
		ino = ext2_find_next_zero_bit((unsigned long *)bitmap_bh->b_data,
					      EXT2_INODES_PER_GROUP(sb), ino);
		if (ino >= EXT2_INODES_PER_GROUP(sb)) {
			/*
			 * Rare race: find_group_xx() decided that there were
			 * free inodes in this group, but by the time we tried
			 * to allocate one, they're all gone.  This can also
			 * occur because the counters which find_group_orlov()
			 * uses are approximate.  So just go and search the
			 * next block group.
			 */
			if (++group == sbi->s_groups_count)
				group = 0;
			continue;
		}
		// 占位
		if (ext2_set_bit_atomic(sb_bgl_lock(sbi, group),
						ino, bitmap_bh->b_data)) {
			/* we lost this inode */
			if (++ino >= EXT2_INODES_PER_GROUP(sb)) {
				/* this group is exhausted, try next group */
				if (++group == sbi->s_groups_count)
					group = 0;
				continue;
			}
			/* try to find free inode in the same group */
			goto repeat_in_this_group;
		}
		goto got;
	}

	/*
	 * Scanned all blockgroups.
	 */
	err = -ENOSPC;
	goto fail;
got:
	// 后面的流程，我觉得都是如字面意思
	// TODO debts维护、ext2_preread_inode()
	mark_buffer_dirty(bitmap_bh);
	if (sb->s_flags & SB_SYNCHRONOUS)
		sync_dirty_buffer(bitmap_bh);
	brelse(bitmap_bh);

	ino += group * EXT2_INODES_PER_GROUP(sb) + 1;
	if (ino < EXT2_FIRST_INO(sb) || ino > le32_to_cpu(es->s_inodes_count)) {
		ext2_error (sb, "ext2_new_inode",
			    "reserved inode or inode > inodes count - "
			    "block_group = %d,inode=%lu", group,
			    (unsigned long) ino);
		err = -EIO;
		goto fail;
	}

	percpu_counter_add(&sbi->s_freeinodes_counter, -1);
	if (S_ISDIR(mode))
		percpu_counter_inc(&sbi->s_dirs_counter);

	spin_lock(sb_bgl_lock(sbi, group));
	le16_add_cpu(&gdp->bg_free_inodes_count, -1);
	if (S_ISDIR(mode)) {
		if (sbi->s_debts[group] < 255)
			sbi->s_debts[group]++;
		le16_add_cpu(&gdp->bg_used_dirs_count, 1);
	} else {
		if (sbi->s_debts[group])
			sbi->s_debts[group]--;
	}
	spin_unlock(sb_bgl_lock(sbi, group));

	mark_buffer_dirty(bh2);
	if (test_opt(sb, GRPID)) {
		inode->i_mode = mode;
		inode->i_uid = current_fsuid();
		inode->i_gid = dir->i_gid;
	} else
		inode_init_owner(inode, dir, mode);

	inode->i_ino = ino;
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	memset(ei->i_data, 0, sizeof(ei->i_data));
	ei->i_flags =
		ext2_mask_flags(mode, EXT2_I(dir)->i_flags & EXT2_FL_INHERITED);
	ei->i_faddr = 0;
	ei->i_frag_no = 0;
	ei->i_frag_size = 0;
	ei->i_file_acl = 0;
	ei->i_dir_acl = 0;
	ei->i_dtime = 0;
	ei->i_block_alloc_info = NULL;
	ei->i_block_group = group;
	ei->i_dir_start_lookup = 0;
	ei->i_state = EXT2_STATE_NEW;
	ext2_set_inode_flags(inode);
	spin_lock(&sbi->s_next_gen_lock);
	inode->i_generation = sbi->s_next_generation++;
	spin_unlock(&sbi->s_next_gen_lock);
	if (insert_inode_locked(inode) < 0) {
		ext2_error(sb, "ext2_new_inode",
			   "inode number already in use - inode=%lu",
			   (unsigned long) ino);
		err = -EIO;
		goto fail;
	}

	err = dquot_initialize(inode);
	if (err)
		goto fail_drop;

	err = dquot_alloc_inode(inode);
	if (err)
		goto fail_drop;

	err = ext2_init_acl(inode, dir);
	if (err)
		goto fail_free_drop;

	err = ext2_init_security(inode, dir, qstr);
	if (err)
		goto fail_free_drop;

	mark_inode_dirty(inode);
	ext2_debug("allocating inode %lu\n", inode->i_ino);
	// TODO
	ext2_preread_inode(inode);
	return inode;

fail_free_drop:
	dquot_free_inode(inode);

fail_drop:
	dquot_drop(inode);
	inode->i_flags |= S_NOQUOTA;
	clear_nlink(inode);
	unlock_new_inode(inode);
	iput(inode);
	return ERR_PTR(err);

fail:
	make_bad_inode(inode);
	iput(inode);
	return ERR_PTR(err);
}


// vfs对应回调
// 道理很简单，分配ext2_inode_info，然后返回内部的vfs inode
static struct inode *ext2_alloc_inode(struct super_block *sb)
{
	struct ext2_inode_info *ei;
	ei = kmem_cache_alloc(ext2_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	ei->i_block_alloc_info = NULL;
	inode_set_iversion(&ei->vfs_inode, 1);
#ifdef CONFIG_QUOTA
	memset(&ei->i_dquot, 0, sizeof(ei->i_dquot));
#endif

	return &ei->vfs_inode;
}

