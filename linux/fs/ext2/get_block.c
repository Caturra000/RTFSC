// 文件：/fs/ext2/inode.c

int ext2_get_block(struct inode *inode, sector_t iblock,
		struct buffer_head *bh_result, int create)
{
	unsigned max_blocks = bh_result->b_size >> inode->i_blkbits;
	bool new = false, boundary = false;
	u32 bno;
	int ret;

	ret = ext2_get_blocks(inode, iblock, max_blocks, &bno, &new, &boundary,
			create);
	// ret > 0才能表示mapped或者allocated的block数目
	if (ret <= 0)
		return ret;

	// 完成映射
	// 1. 打上map flag
	// 2. bh_result->b_bdev =  inode->i_sb->s_bdev
	// 3. bh_result->b_blocknr = bno
	// 4. bh_result->b_size = inode->i_sb->s_blocksize
	map_bh(bh_result, inode->i_sb, bno);
	bh_result->b_size = (ret << inode->i_blkbits);
	if (new)
		set_buffer_new(bh_result);
	if (boundary)
		set_buffer_boundary(bh_result);
	return 0;

}

/*
 * Allocation strategy is simple: if we have to allocate something, we will
 * have to go the whole way to leaf. So let's do it before attaching anything
 * to tree, set linkage between the newborn blocks, write them if sync is
 * required, recheck the path, free and repeat if check fails, otherwise
 * set the last missing link (that will protect us from any truncate-generated
 * removals - all blocks on the path are immune now) and possibly force the
 * write on the parent block.
 * That has a nice additional property: no special recovery from the failed
 * allocations is needed - we simply release blocks and do not touch anything
 * reachable from inode.
 *
 * `handle' can be NULL if create == 0.
 *
 * return > 0, # of blocks mapped or allocated.
 * return = 0, if plain lookup failed.
 * return < 0, error case.
 */
// 主要涉及分配相关的流程
// 1. 求出并获取存储路径
// 2. 需要分配的块的个数和物理地址
// 3. 执行块分配
// 也可能只是lookup，这样流程就相对简单点
static int ext2_get_blocks(struct inode *inode,
			   sector_t iblock, unsigned long maxblocks,
			   u32 *bno, bool *new, bool *boundary,
			   int create)
{
	int err;
	int offsets[4];
	Indirect chain[4];
	Indirect *partial;
	ext2_fsblk_t goal;
	int indirect_blks;
	int blocks_to_boundary = 0;
	int depth;
	struct ext2_inode_info *ei = EXT2_I(inode);
	int count = 0;
	ext2_fsblk_t first_block = 0;

	BUG_ON(maxblocks == 0);

	// 根据逻辑地址iblock求出间接块路径，存储在offset
	// 正常获取到结果的depth取值范围是[1,4]，比如说，层数为1就是直接块
	depth = ext2_block_to_path(inode,iblock,offsets,&blocks_to_boundary);

	if (depth == 0)
		return -EIO;

	// 解析对应的间接块信息（chain of indirect blocks），填充chain
	// Indirect数组chain用于存储结果
	// Indirect结构体是一个三元组<p, key, bh>，类型分别为<le32*, le32, buffer_head*>
	// - p为block地址，key为值，至于为什么要分开为两个成员而不是通过*p来得到值，可以看verify_chain()的用法
	// - bh就是这个block所在的buffer_head
	// partial也是一个Indirect指针
	// - 如果为NULL，那就是解析成功
	// - 否则的话，就是解析不完全，返回最后一个解析的三元组
	// Indirect具体用法见ext2_get_branch注释
	partial = ext2_get_branch(inode, depth, offsets, chain, &err);
	/* Simplest case - block found, no allocation needed */
	if (!partial) {
		first_block = le32_to_cpu(chain[depth - 1].key);
		count++;
		/*map more blocks*/
		while (count < maxblocks && count <= blocks_to_boundary) {
			ext2_fsblk_t blk;

			if (!verify_chain(chain, chain + depth - 1)) {
				/*
				 * Indirect block might be removed by
				 * truncate while we were reading it.
				 * Handling of that case: forget what we've
				 * got now, go to reread.
				 */
				err = -EAGAIN;
				count = 0;
				partial = chain + depth - 1;
				break;
			}
			blk = le32_to_cpu(*(chain[depth-1].p + count));
			if (blk == first_block + count)
				count++;
			else
				break;
		}
		if (err != -EAGAIN)
			goto got_it;
	}

	/* Next simple case - plain lookup or failed read of indirect block */
	if (!create || err == -EIO)
		goto cleanup;

	mutex_lock(&ei->truncate_mutex);
	/*
	 * If the indirect block is missing while we are reading
	 * the chain(ext2_get_branch() returns -EAGAIN err), or
	 * if the chain has been changed after we grab the semaphore,
	 * (either because another process truncated this branch, or
	 * another get_block allocated this branch) re-grab the chain to see if
	 * the request block has been allocated or not.
	 *
	 * Since we already block the truncate/other get_block
	 * at this point, we will have the current copy of the chain when we
	 * splice the branch into the tree.
	 */
	if (err == -EAGAIN || !verify_chain(chain, partial)) {
		while (partial > chain) {
			brelse(partial->bh);
			partial--;
		}
		partial = ext2_get_branch(inode, depth, offsets, chain, &err);
		if (!partial) {
			count++;
			mutex_unlock(&ei->truncate_mutex);
			if (err)
				goto cleanup;
			goto got_it;
		}
	}

	/*
	 * Okay, we need to do block allocation.  Lazily initialize the block
	 * allocation info here if necessary
	*/
	if (S_ISREG(inode->i_mode) && (!ei->i_block_alloc_info))
		ext2_init_block_alloc_info(inode);

	// 找一个适合分配的地方
	goal = ext2_find_goal(inode, iblock, partial);

	/* the number of blocks need to allocate for [d,t]indirect blocks */
	indirect_blks = (chain + depth) - partial - 1;
	/*
	 * Next look up the indirect map to count the totoal number of
	 * direct blocks to allocate for this branch.
	 */
	// 求需要分配的block个数
	count = ext2_blks_to_allocate(partial, indirect_blks,
					maxblocks, blocks_to_boundary);
	/*
	 * XXX ???? Block out ext2_truncate while we alter the tree
	 */
	err = ext2_alloc_branch(inode, indirect_blks, &count, goal,
				offsets + (partial - chain), partial);

	if (err) {
		mutex_unlock(&ei->truncate_mutex);
		goto cleanup;
	}

	if (IS_DAX(inode)) {
		/*
		 * We must unmap blocks before zeroing so that writeback cannot
		 * overwrite zeros with stale data from block device page cache.
		 */
		clean_bdev_aliases(inode->i_sb->s_bdev,
				   le32_to_cpu(chain[depth-1].key),
				   count);
		/*
		 * block must be initialised before we put it in the tree
		 * so that it's not found by another thread before it's
		 * initialised
		 */
		err = sb_issue_zeroout(inode->i_sb,
				le32_to_cpu(chain[depth-1].key), count,
				GFP_NOFS);
		if (err) {
			mutex_unlock(&ei->truncate_mutex);
			goto cleanup;
		}
	}
	*new = true;

	ext2_splice_branch(inode, iblock, partial, indirect_blks, count);
	mutex_unlock(&ei->truncate_mutex);
got_it:
	if (count > blocks_to_boundary)
		*boundary = true;
	err = count;
	/* Clean up and exit */
	partial = chain + depth - 1;	/* the whole chain */
cleanup:
	while (partial > chain) {
		brelse(partial->bh);
		partial--;
	}
	if (err > 0)
		*bno = le32_to_cpu(chain[depth-1].key);
	return err;
}




/**
 *	ext2_block_to_path - parse the block number into array of offsets
 *	@inode: inode in question (we are only interested in its superblock)
 *	@i_block: block number to be parsed
 *	@offsets: array to store the offsets in
 *      @boundary: set this non-zero if the referred-to block is likely to be
 *             followed (on disk) by an indirect block.
 *	To store the locations of file's data ext2 uses a data structure common
 *	for UNIX filesystems - tree of pointers anchored in the inode, with
 *	data blocks at leaves and indirect blocks in intermediate nodes.
 *	This function translates the block number into path in that tree -
 *	return value is the path length and @offsets[n] is the offset of
 *	pointer to (n+1)th node in the nth one. If @block is out of range
 *	(negative or too large) warning is printed and zero returned.
 *
 *	Note: function doesn't find node addresses, so no IO is needed. All
 *	we need to know is the capacity of indirect blocks (taken from the
 *	inode->i_sb).
 */

/*
 * Portability note: the last comparison (check that we fit into triple
 * indirect block) is spelled differently, because otherwise on an
 * architecture with 32-bit longs and 8Kb pages we might get into trouble
 * if our filesystem had 8Kb blocks. We might use long long, but that would
 * kill us on x86. Oh, well, at least the sign propagation does not matter -
 * i_block would have to be negative in the very beginning, so we would not
 * get there at all.
 */

static int ext2_block_to_path(struct inode *inode,
			long i_block, int offsets[4], int *boundary)
{
	// inode->i_sb->s_blocksize / sizeof (__u32)
	int ptrs = EXT2_ADDR_PER_BLOCK(inode->i_sb);
	int ptrs_bits = EXT2_ADDR_PER_BLOCK_BITS(inode->i_sb);
	// 直接块的数量，常量12
	const long direct_blocks = EXT2_NDIR_BLOCKS,
		indirect_blocks = ptrs,
		double_blocks = (1 << (ptrs_bits * 2));
	int n = 0;
	int final = 0;

	if (i_block < 0) {
		ext2_msg(inode->i_sb, KERN_WARNING,
			"warning: %s: block < 0", __func__);
	// 下面是直接 / 多级间接的返回形式
	} else if (i_block < direct_blocks) {
		offsets[n++] = i_block;
		final = direct_blocks;
	} else if ( (i_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT2_IND_BLOCK;
		offsets[n++] = i_block;
		final = ptrs;
	} else if ((i_block -= indirect_blocks) < double_blocks) {
		offsets[n++] = EXT2_DIND_BLOCK;
		offsets[n++] = i_block >> ptrs_bits;
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT2_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++] = i_block & (ptrs - 1);
		final = ptrs;
	} else {
		ext2_msg(inode->i_sb, KERN_WARNING,
			"warning: %s: block is too big", __func__);
	}
	if (boundary)
		*boundary = final - 1 - (i_block & (ptrs - 1));

	return n;
}






/**
 *	ext2_get_branch - read the chain of indirect blocks leading to data
 *	@inode: inode in question
 *	@depth: depth of the chain (1 - direct pointer, etc.)
 *	@offsets: offsets of pointers in inode/indirect blocks
 *	@chain: place to store the result
 *	@err: here we store the error value
 *
 *	Function fills the array of triples <key, p, bh> and returns %NULL
 *	if everything went OK or the pointer to the last filled triple
 *	(incomplete one) otherwise. Upon the return chain[i].key contains
 *	the number of (i+1)-th block in the chain (as it is stored in memory,
 *	i.e. little-endian 32-bit), chain[i].p contains the address of that
 *	number (it points into struct inode for i==0 and into the bh->b_data
 *	for i>0) and chain[i].bh points to the buffer_head of i-th indirect
 *	block for i>0 and NULL for i==0. In other words, it holds the block
 *	numbers of the chain, addresses they were taken from (and where we can
 *	verify that chain did not change) and buffer_heads hosting these
 *	numbers.
 *
 *	Function stops when it stumbles upon zero pointer (absent block)
 *		(pointer to last triple returned, *@err == 0)
 *	or when it gets an IO error reading an indirect block
 *		(ditto, *@err == -EIO)
 *	or when it notices that chain had been changed while it was reading
 *		(ditto, *@err == -EAGAIN)
 *	or when it reads all @depth-1 indirect blocks successfully and finds
 *	the whole chain, all way to the data (returns %NULL, *err == 0).
 */
static Indirect *ext2_get_branch(struct inode *inode,
				 int depth,
				 int *offsets,
				 Indirect chain[4],
				 int *err)
{
	struct super_block *sb = inode->i_sb;
	Indirect *p = chain;
	struct buffer_head *bh;

	*err = 0;
	/* i_data is not going away, no lock needed */
	// add_chain的执行过程，简单对chain填充
	// （这里bh为NULL, v为EXT2_I(inode)->i_data + *offsets）
	// chain->p = v
	// chain->key = *chain->p
	// chain->bh = bh
	add_chain (chain, NULL, EXT2_I(inode)->i_data + *offsets);
	if (!p->key)
		goto no_block;
	// 就是不断地填满p（或者叫partial）
	while (--depth) {
		bh = sb_bread(sb, le32_to_cpu(p->key));
		if (!bh)
			goto failure;
		read_lock(&EXT2_I(inode)->i_meta_lock);
		if (!verify_chain(chain, p))
			goto changed;
		add_chain(++p, bh, (__le32*)bh->b_data + *++offsets);
		read_unlock(&EXT2_I(inode)->i_meta_lock);
		if (!p->key)
			goto no_block;
	}
	return NULL;

changed:
	read_unlock(&EXT2_I(inode)->i_meta_lock);
	brelse(bh);
	*err = -EAGAIN;
	goto no_block;
failure:
	*err = -EIO;
no_block:
	return p;
}





/**
 *	ext2_find_goal - find a preferred place for allocation.
 *	@inode: owner
 *	@block:  block we want
 *	@partial: pointer to the last triple within a chain
 *
 *	Returns preferred place for a block (the goal).
 */

static inline ext2_fsblk_t ext2_find_goal(struct inode *inode, long block,
					  Indirect *partial)
{
	struct ext2_block_alloc_info *block_i;

	block_i = EXT2_I(inode)->i_block_alloc_info;

	/*
	 * try the heuristic for sequential allocation,
	 * failing that at least try to get decent locality.
	 */
	if (block_i && (block == block_i->last_alloc_logical_block + 1)
		&& (block_i->last_alloc_physical_block != 0)) {
		return block_i->last_alloc_physical_block + 1;
	}

	return ext2_find_near(inode, partial);
}




/**
 *	ext2_blks_to_allocate: Look up the block map and count the number
 *	of direct blocks need to be allocated for the given branch.
 *
 * 	@branch: chain of indirect blocks
 *	@k: number of blocks need for indirect blocks
 *	@blks: number of data blocks to be mapped.
 *	@blocks_to_boundary:  the offset in the indirect block
 *
 *	return the total number of blocks to be allocate, including the
 *	direct and indirect blocks.
 */
static int
ext2_blks_to_allocate(Indirect * branch, int k, unsigned long blks,
		int blocks_to_boundary)
{
	unsigned long count = 0;

	/*
	 * Simple case, [t,d]Indirect block(s) has not allocated yet
	 * then it's clear blocks on that path have not allocated
	 */
	if (k > 0) {
		/* right now don't hanel cross boundary allocation */
		if (blks < blocks_to_boundary + 1)
			count += blks;
		else
			count += blocks_to_boundary + 1;
		return count;
	}

	count++;
	while (count < blks && count <= blocks_to_boundary
		&& le32_to_cpu(*(branch[0].p + count)) == 0) {
		count++;
	}
	return count;
}



/**
 *	ext2_alloc_branch - allocate and set up a chain of blocks.
 *	@inode: owner
 *	@num: depth of the chain (number of blocks to allocate)
 *	@offsets: offsets (in the blocks) to store the pointers to next.
 *	@branch: place to store the chain in.
 *
 *	This function allocates @num blocks, zeroes out all but the last one,
 *	links them into chain and (if we are synchronous) writes them to disk.
 *	In other words, it prepares a branch that can be spliced onto the
 *	inode. It stores the information about that chain in the branch[], in
 *	the same format as ext2_get_branch() would do. We are calling it after
 *	we had read the existing part of chain and partial points to the last
 *	triple of that (one with zero ->key). Upon the exit we have the same
 *	picture as after the successful ext2_get_block(), except that in one
 *	place chain is disconnected - *branch->p is still zero (we did not
 *	set the last link), but branch->key contains the number that should
 *	be placed into *branch->p to fill that gap.
 *
 *	If allocation fails we free all blocks we've allocated (and forget
 *	their buffer_heads) and return the error value the from failed
 *	ext2_alloc_block() (normally -ENOSPC). Otherwise we set the chain
 *	as described above and return 0.
 */

static int ext2_alloc_branch(struct inode *inode,
			int indirect_blks, int *blks, ext2_fsblk_t goal,
			int *offsets, Indirect *branch)
{
	int blocksize = inode->i_sb->s_blocksize;
	int i, n = 0;
	int err = 0;
	struct buffer_head *bh;
	int num;
	ext2_fsblk_t new_blocks[4];
	ext2_fsblk_t current_block;

	num = ext2_alloc_blocks(inode, goal, indirect_blks,
				*blks, new_blocks, &err);
	if (err)
		return err;

	branch[0].key = cpu_to_le32(new_blocks[0]);
	/*
	 * metadata blocks and data blocks are allocated.
	 */
	for (n = 1; n <= indirect_blks;  n++) {
		/*
		 * Get buffer_head for parent block, zero it out
		 * and set the pointer to new one, then send
		 * parent to disk.
		 */
		bh = sb_getblk(inode->i_sb, new_blocks[n-1]);
		if (unlikely(!bh)) {
			err = -ENOMEM;
			goto failed;
		}
		branch[n].bh = bh;
		lock_buffer(bh);
		memset(bh->b_data, 0, blocksize);
		branch[n].p = (__le32 *) bh->b_data + offsets[n];
		branch[n].key = cpu_to_le32(new_blocks[n]);
		*branch[n].p = branch[n].key;
		if ( n == indirect_blks) {
			current_block = new_blocks[n];
			/*
			 * End of chain, update the last new metablock of
			 * the chain to point to the new allocated
			 * data blocks numbers
			 */
			for (i=1; i < num; i++)
				*(branch[n].p + i) = cpu_to_le32(++current_block);
		}
		set_buffer_uptodate(bh);
		unlock_buffer(bh);
		mark_buffer_dirty_inode(bh, inode);
		/* We used to sync bh here if IS_SYNC(inode).
		 * But we now rely upon generic_write_sync()
		 * and b_inode_buffers.  But not for directories.
		 */
		if (S_ISDIR(inode->i_mode) && IS_DIRSYNC(inode))
			sync_dirty_buffer(bh);
	}
	*blks = num;
	return err;

failed:
	for (i = 1; i < n; i++)
		bforget(branch[i].bh);
	for (i = 0; i < indirect_blks; i++)
		ext2_free_blocks(inode, new_blocks[i], 1);
	ext2_free_blocks(inode, new_blocks[i], num);
	return err;
}
