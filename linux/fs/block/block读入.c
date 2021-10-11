// 文件：/fs/buffer.c
// NOTE：这里的流程经常像二叉树一样的展开，线性的跟踪代码基本没法读（函数名太乱了），因此尝试打上标记
//       跟踪在IDE里直接ctrl就好了

/**
 *  __bread_gfp() - reads a specified block and returns the bh
 *  @bdev: the block_device to read from
 *  @block: number of block
 *  @size: size (in bytes) to read
 *  @gfp: page allocation flag
 *
 *  Reads a specified block, and returns buffer head that contains it.
 *  The page cache can be allocated from non-movable area
 *  not to prevent page migration if you set gfp to zero.
 *  It returns NULL if the block was unreadable.
 */
// 接口，读入bdev里面的块，返回映射其数据的buffer head
struct buffer_head *
__bread_gfp(struct block_device *bdev, sector_t block,
		   unsigned size, gfp_t gfp)
{
	// ##flag #0
	struct buffer_head *bh = __getblk_gfp(bdev, block, size, gfp);

	if (likely(bh) && !buffer_uptodate(bh))
		// ##flag #1
		// 简单来说就是发出request
		bh = __bread_slow(bh);
	return bh;
}

/*
 * __getblk_gfp() will locate (and, if necessary, create) the buffer_head
 * which corresponds to the passed block_device, block and size. The
 * returned buffer has its reference count incremented.
 *
 * __getblk_gfp() will lock up the machine if grow_dev_page's
 * try_to_free_buffers() attempt is failing.  FIXME, perhaps?
 */
// ##flag #0
// 从注释来看，这个函数会尝试找到/创建对应设备的（多个）块的buffer head
struct buffer_head *
__getblk_gfp(struct block_device *bdev, sector_t block,
	     unsigned size, gfp_t gfp)
{
	// ##flag #2
	// 尝试往LRU / page cache里找，找不到就返回NULL
	struct buffer_head *bh = __find_get_block(bdev, block, size);

	might_sleep();
	if (bh == NULL)
		// ##flag #3
		bh = __getblk_slow(bdev, block, size, gfp);
	return bh;
}

/*
 * Perform a pagecache lookup for the matching buffer.  If it's there, refresh
 * it in the LRU and mark it as accessed.  If it is not present then return
 * NULL
 */
// ##flag #2
// 往缓存里找，这里额外添加了一层LRU
// 找不到就是返回NULL
struct buffer_head *
__find_get_block(struct block_device *bdev, sector_t block, unsigned size)
{
	// 在this cpu lru里查找
	// 可能位于bh_lrus.bhs
	// 内部基本就是并发接口的使用了
	struct buffer_head *bh = lookup_bh_lru(bdev, block, size);

	if (bh == NULL) {
		/* __find_get_block_slow will mark the page accessed */
		// ##flag #4
		// 在page cache（既address_space）里找
		bh = __find_get_block_slow(bdev, block);
		if (bh)
			// 找到了就插入到lru
			bh_lru_install(bh);
	} else
		// 意义不太明白，总之对bh->b_page进行mark page accessed操作
		touch_buffer(bh);

	return bh;
}

/*
 * Various filesystems appear to want __find_get_block to be non-blocking.
 * But it's the page lock which protects the buffers.  To get around this,
 * we get exclusion from try_to_free_buffers with the blockdev mapping's
 * private_lock.
 *
 * Hack idea: for the blockdev mapping, private_lock contention
 * may be quite high.  This code could TryLock the page, and if that
 * succeeds, there is no need to take private_lock.
 */
static struct buffer_head *
// ##flag #4
// 虽然说是slow，但起码也是从page cache里查找bh
// 如果没找到，那就是返回NULL
__find_get_block_slow(struct block_device *bdev, sector_t block)
{
	struct inode *bd_inode = bdev->bd_inode;
	struct address_space *bd_mapping = bd_inode->i_mapping;
	struct buffer_head *ret = NULL;
	pgoff_t index;
	struct buffer_head *bh;
	struct buffer_head *head;
	struct page *page;
	int all_mapped = 1;

	// block对应的page index
	index = block >> (PAGE_SHIFT - bd_inode->i_blkbits);
	// 往mapping里掏出page
	page = find_get_page_flags(bd_mapping, index, FGP_ACCESSED);
	if (!page)
		goto out;

	spin_lock(&bd_mapping->private_lock);
	if (!page_has_buffers(page))
		goto out_unlock;
	// TODO 这种形式的page与bh的关联是从哪里构造得到的？
	head = page_buffers(page);
	bh = head;
	// 双向循环链表形式遍历
	do {
		if (!buffer_mapped(bh))
			all_mapped = 0;
		// 找到对应的block了？
		else if (bh->b_blocknr == block) {
			ret = bh;
			get_bh(bh);
			goto out_unlock;
		}
		bh = bh->b_this_page;
	} while (bh != head);

	/* we might be here because some of the buffers on this page are
	 * not mapped.  This is due to various races between
	 * file io on the block device and getblk.  It gets dealt with
	 * elsewhere, don't buffer_error if we had some unmapped buffers
	 */
	if (all_mapped) {
		printk("__find_get_block_slow() failed. "
			"block=%llu, b_blocknr=%llu\n",
			(unsigned long long)block,
			(unsigned long long)bh->b_blocknr);
		printk("b_state=0x%08lx, b_size=%zu\n",
			bh->b_state, bh->b_size);
		printk("device %pg blocksize: %d\n", bdev,
			1 << bd_inode->i_blkbits);
	}
out_unlock:
	spin_unlock(&bd_mapping->private_lock);
	put_page(page);
out:
	return ret;
}

// 关键流程到此为止，下面的为分支函数


static struct buffer_head *
// ##flag #3
__getblk_slow(struct block_device *bdev, sector_t block,
	     unsigned size, gfp_t gfp)
{
	/* Size must be multiple of hard sectorsize */
	if (unlikely(size & (bdev_logical_block_size(bdev)-1) ||
			(size < 512 || size > PAGE_SIZE))) {
		printk(KERN_ERR "getblk(): invalid block size %d requested\n",
					size);
		printk(KERN_ERR "logical block size: %d\n",
					bdev_logical_block_size(bdev));

		dump_stack();
		return NULL;
	}

	for (;;) {
		struct buffer_head *bh;
		int ret;

		// ##flag #2
		bh = __find_get_block(bdev, block, size);
		if (bh)
			return bh;

		// TODO
		ret = grow_buffers(bdev, block, size, gfp);
		if (ret < 0)
			return NULL;
	}
}

// ##flag #1
static struct buffer_head *__bread_slow(struct buffer_head *bh)
{
	lock_buffer(bh);
	if (buffer_uptodate(bh)) {
		unlock_buffer(bh);
		return bh;
	} else {
		get_bh(bh);
		bh->b_end_io = end_buffer_read_sync;
		submit_bh(REQ_OP_READ, 0, bh);
		wait_on_buffer(bh);
		if (buffer_uptodate(bh))
			return bh;
	}
	brelse(bh);
	return NULL;
}