// 文件：/fs/minix/inode.c
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
static int get_block(struct inode * inode, sector_t block,
			struct buffer_head *bh, int create)
{
	int err = -EIO;
	int offsets[DEPTH];
	Indirect chain[DEPTH];
	Indirect *partial;
	int left;
	int depth = block_to_path(inode, block, offsets);

	if (depth == 0)
		goto out;

reread:
	partial = get_branch(inode, depth, offsets, chain, &err);

	/* Simplest case - block found, no allocation needed */
	if (!partial) {
got_it:
		map_bh(bh, inode->i_sb, block_to_cpu(chain[depth-1].key));
		/* Clean up and exit */
		partial = chain+depth-1; /* the whole chain */
		goto cleanup;
	}

	/* Next simple case - plain lookup or failed read of indirect block */
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
	err = alloc_branch(inode, left, offsets+(partial-chain), partial);
	if (err)
		goto cleanup;

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