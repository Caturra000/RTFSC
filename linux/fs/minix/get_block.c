// 文件：/fs/minix/inode.c

// 将文件内部的块编号block转换为全局的逻辑块编号
static int minix_get_block(struct inode *inode, sector_t block,
		    struct buffer_head *bh_result, int create)
{
	if (INODE_VERSION(inode) == MINIX_V1)
		return V1_minix_get_block(inode, block, bh_result, create);
	else
		return V2_minix_get_block(inode, block, bh_result, create);
}

int V2_minix_get_block(struct inode * inode, long block,
			struct buffer_head *bh_result, int create)
{
	return get_block(inode, block, bh_result, create);
}

// 文件：/fs/minix/itree_common.c
// 对比ext2_get_block是基本一致的实现方式
// 同样是block_to_path -> get_branch -> alloc_branch -> splice_branch
static int get_block(struct inode * inode, sector_t block,
			struct buffer_head *bh, int create)
{
	int err = -EIO;
	// offsets[0]为inode内的下标
	// 其余的是间接块block的下标
	// 比如block==5，那么offset[0]就是5，即可定位到izone[5]然后拿到直接块
	int offsets[DEPTH];
	// 用于算出最终的结果
	Indirect chain[DEPTH];
	// 记录断链情况（比如间接块寻址没法完成），空指针则表示为完整链路
	Indirect *partial;
	int left;
	// 根据block号求出各层offset
	int depth = block_to_path(inode, block, offsets);

	if (depth == 0)
		goto out;

reread:
	// 从offset得到映射信息，存储于chain
	partial = get_branch(inode, depth, offsets, chain, &err);

	/* Simplest case - block found, no allocation needed */
	if (!partial) {
got_it:
		// 完成映射bdev / blocknr / size
		map_bh(bh, inode->i_sb, block_to_cpu(chain[depth-1].key));
		/* Clean up and exit */
		partial = chain+depth-1; /* the whole chain */
		// 流程就这么结束了，get_block也没有对bh->b_data干什么事情，只是想拿到最终的blocknr
		goto cleanup;
	}

	/* Next simple case - plain lookup or failed read of indirect block */
	// 如果是read流程，那么出现问题是不会create的，而是上报err结束
	if (!create || err == -EIO) {
cleanup:
		while (partial > chain) {
			brelse(partial->bh);
			partial--;
		}
out:
		return err;
	}

	/*
	 * Indirect block might be removed by truncate while we were
	 * reading it. Handling of that case (forget what we've got and
	 * reread) is taken out of the main path.
	 */
	if (err == -EAGAIN)
		goto changed;

	left = (chain + depth) - partial;
	// 补全断链，需要补的数目为left
	err = alloc_branch(inode, left, offsets+(partial-chain), partial);
	if (err)
		goto cleanup;

	// 将已存在的和刚补上的连接起来
	if (splice_branch(inode, chain, partial, left) < 0)
		goto changed;

	set_buffer_new(bh);
	goto got_it;

changed:
	while (partial > chain) {
		brelse(partial->bh);
		partial--;
	}
	goto reread;
}

static int block_to_path(struct inode * inode, long block, int offsets[DEPTH])
{
	int n = 0;
	struct super_block *sb = inode->i_sb;

	if (block < 0) {
		printk("MINIX-fs: block_to_path: block %ld < 0 on dev %pg\n",
			block, sb->s_bdev);
	} else if ((u64)block * (u64)sb->s_blocksize >=
			minix_sb(sb)->s_max_size) {
		if (printk_ratelimit())
			printk("MINIX-fs: block_to_path: "
			       "block %ld too big on dev %pg\n",
				block, sb->s_bdev);
	// DIRCOUNT == 7
	// 这些offsets应该是块内偏移
	} else if (block < DIRCOUNT) {
		offsets[n++] = block;
	// INDIRCOUNT(sb) == (1 << ((sb)->s_blocksize_bits - 2))
	} else if ((block -= DIRCOUNT) < INDIRCOUNT(sb)) {
		offsets[n++] = DIRCOUNT;
		offsets[n++] = block;
	} else if ((block -= INDIRCOUNT(sb)) < INDIRCOUNT(sb) * INDIRCOUNT(sb)) {
		offsets[n++] = DIRCOUNT + 1;
		offsets[n++] = block / INDIRCOUNT(sb);
		offsets[n++] = block % INDIRCOUNT(sb);
	} else {
		block -= INDIRCOUNT(sb) * INDIRCOUNT(sb);
		offsets[n++] = DIRCOUNT + 2;
		offsets[n++] = (block / INDIRCOUNT(sb)) / INDIRCOUNT(sb);
		offsets[n++] = (block / INDIRCOUNT(sb)) % INDIRCOUNT(sb);
		offsets[n++] = block % INDIRCOUNT(sb);
	}
	return n;
}

static inline Indirect *get_branch(struct inode *inode,
					int depth,
					int *offsets,
					Indirect chain[DEPTH],
					int *err)
{
	struct super_block *sb = inode->i_sb;
	Indirect *p = chain;
	struct buffer_head *bh;

	*err = 0;
	/* i_data is not going away, no lock needed */
	// 至少第一层是有的
	// i_data读取u.i2_data（__u32[16]数组，一般用0-9，剩下的做padding），数据来自raw_inode的i_zone
	// i_zone就是用于存储位置信息
	add_chain (chain, NULL, i_data(inode) + *offsets);
	if (!p->key)
		goto no_block;
	while (--depth) {
		// 读入内容到bh
		// 如果cache没命中，则会有IO产生
		bh = sb_bread(sb, block_to_cpu(p->key));
		if (!bh)
			goto failure;
		read_lock(&pointers_lock);
		if (!verify_chain(chain, p))
			goto changed;
		add_chain(++p, bh, (block_t *)bh->b_data + *++offsets);
		read_unlock(&pointers_lock);
		if (!p->key)
			goto no_block;
	}
	return NULL;

changed:
	read_unlock(&pointers_lock);
	brelse(bh);
	*err = -EAGAIN;
	goto no_block;
failure:
	*err = -EIO;
no_block:
	return p;
}

static int alloc_branch(struct inode *inode,
			     int num,
			     int *offsets,
			     Indirect *branch)
{
	int n = 0;
	int i;
	int parent = minix_new_block(inode);

	branch[0].key = cpu_to_block(parent);
	// branch[0].p是没有赋值的，按某fs的做法猜测是考虑崩溃一致性
	// 可以做到不到最后一刻（splice_branch）不管怎么崩溃都不会产生问题
	if (parent) for (n = 1; n < num; n++) {
		struct buffer_head *bh;
		/* Allocate the next block */
		int nr = minix_new_block(inode);
		if (!nr)
			break;
		branch[n].key = cpu_to_block(nr);
		bh = sb_getblk(inode->i_sb, parent);
		lock_buffer(bh);
		memset(bh->b_data, 0, bh->b_size);
		branch[n].bh = bh;
		branch[n].p = (block_t*) bh->b_data + offsets[n];
		*branch[n].p = branch[n].key;
		set_buffer_uptodate(bh);
		unlock_buffer(bh);
		mark_buffer_dirty_inode(bh, inode);
		parent = nr;
	}
	if (n == num)
		return 0;

	/* Allocation failed, free what we already allocated */
	for (i = 1; i < n; i++)
		bforget(branch[i].bh);
	for (i = 0; i < n; i++)
		minix_free_block(inode, block_to_cpu(branch[i].key));
	return -ENOSPC;
}

static inline int splice_branch(struct inode *inode,
				     Indirect chain[DEPTH],
				     Indirect *where,
				     int num)
{
	int i;

	write_lock(&pointers_lock);

	/* Verify that place we are splicing to is still there and vacant */
	if (!verify_chain(chain, where-1) || *where->p)
		goto changed;

	*where->p = where->key;

	write_unlock(&pointers_lock);

	/* We are done with atomic stuff, now do the rest of housekeeping */

	inode->i_ctime = current_time(inode);

	/* had we spliced it onto indirect block? */
	if (where->bh)
		mark_buffer_dirty_inode(where->bh, inode);

	mark_inode_dirty(inode);
	return 0;

changed:
	write_unlock(&pointers_lock);
	for (i = 1; i < num; i++)
		bforget(where[i].bh);
	for (i = 0; i < num; i++)
		minix_free_block(inode, block_to_cpu(where[i].key));
	return -EAGAIN;
}
