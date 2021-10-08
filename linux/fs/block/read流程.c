// 文件：/fs/buffer.c

/*
 * Generic "read page" function for block devices that have the normal
 * get_block functionality. This is most of the block device filesystems.
 * Reads the page asynchronously --- the unlock_buffer() and
 * set/clear_buffer_uptodate() functions propagate buffer state into the
 * page struct once IO has completed.
 */
// 在minix fs中被minix_readpage直接调用
// 也会被do_mpage_readpage流程调用（下面有代码）
// get_block由具体文件系统提供
int block_read_full_page(struct page *page, get_block_t *get_block)
{
	// 首先找到inode
	struct inode *inode = page->mapping->host;
	sector_t iblock, lblock;
	struct buffer_head *bh, *head, *arr[MAX_BUF_PER_PAGE];
	unsigned int blocksize, bbits;
	int nr, i;
	int fully_mapped = 1;

	// 创建/找到bh
	head = create_page_buffers(page, inode, 0);
	blocksize = head->b_size;
	bbits = block_size_bits(blocksize);

	iblock = (sector_t)page->index << (PAGE_SHIFT - bbits);
	lblock = (i_size_read(inode)+blocksize-1) >> bbits;
	bh = head;
	nr = 0;
	i = 0;

	// 注意，这里continue的都是uptodate的场合
	do {
		if (buffer_uptodate(bh))
			continue;

		if (!buffer_mapped(bh)) {
			int err = 0;

			fully_mapped = 0;
			if (iblock < lblock) {
				WARN_ON(bh->b_size != blocksize);
				// 既不uptodate，也不mapped，那就用iblock问具体文件系统
				err = get_block(inode, iblock, bh, 0);
				if (err)
					SetPageError(page);
			}
			// 超过文件范围了？
			if (!buffer_mapped(bh)) {
				zero_user(page, i * blocksize, blocksize);
				if (!err)
					set_buffer_uptodate(bh);
				continue;
			}
			/*
			 * get_block() might have updated the buffer
			 * synchronously
			 */
			if (buffer_uptodate(bh))
				continue;
		}
		// 已建立映射，但内容不够uptodate
		arr[nr++] = bh;
	// b_this_page是一个循环list
	} while (i++, iblock++, (bh = bh->b_this_page) != head);

	// TODO
	if (fully_mapped)
		SetPageMappedToDisk(page);

	// 全都是uptodate
	if (!nr) {
		/*
		 * All buffers are uptodate - we can set the page uptodate
		 * as well. But not if get_block() returned an error.
		 */
		if (!PageError(page))
			SetPageUptodate(page);
		unlock_page(page);
		return 0;
	}

	/* Stage two: lock the buffers */
	for (i = 0; i < nr; i++) {
		bh = arr[i];
		lock_buffer(bh);
		mark_buffer_async_read(bh);
	}

	/*
	 * Stage 3: start the IO.  Check for uptodateness
	 * inside the buffer lock in case another process reading
	 * the underlying blockdev brought it uptodate (the sct fix).
	 */
	for (i = 0; i < nr; i++) {
		bh = arr[i];
		// 临走前再检查一遍
		if (buffer_uptodate(bh))
			end_buffer_async_read(bh, 1);
		else
			submit_bh(REQ_OP_READ, 0, bh);
	}
	return 0;
}


















////////////////////////////////////////////////////////






// 文件：/fs/mpage.c

/**
 * mpage_readpages - populate an address space with some pages & start reads against them
 * @mapping: the address_space
 * @pages: The address of a list_head which contains the target pages.  These
 *   pages have their ->index populated and are otherwise uninitialised.
 *   The page at @pages->prev has the lowest file offset, and reads should be
 *   issued in @pages->prev to @pages->next order.
 * @nr_pages: The number of pages at *@pages
 * @get_block: The filesystem's block mapper function.
 *
 * This function walks the pages and the blocks within each page, building and
 * emitting large BIOs.
 *
 * If anything unusual happens, such as:
 *
 * - encountering a page which has buffers
 * - encountering a page which has a non-hole after a hole
 * - encountering a page with non-contiguous blocks
 *
 * then this code just gives up and calls the buffer_head-based read function.
 * It does handle a page which has holes at the end - that is a common case:
 * the end-of-file on blocksize < PAGE_SIZE setups.
 *
 * BH_Boundary explanation:
 *
 * There is a problem.  The mpage read code assembles several pages, gets all
 * their disk mappings, and then submits them all.  That's fine, but obtaining
 * the disk mappings may require I/O.  Reads of indirect blocks, for example.
 *
 * So an mpage read of the first 16 blocks of an ext2 file will cause I/O to be
 * submitted in the following order:
 *
 * 	12 0 1 2 3 4 5 6 7 8 9 10 11 13 14 15 16
 *
 * because the indirect block has to be read to get the mappings of blocks
 * 13,14,15,16.  Obviously, this impacts performance.
 *
 * So what we do it to allow the filesystem's get_block() function to set
 * BH_Boundary when it maps block 11.  BH_Boundary says: mapping of the block
 * after this one will require I/O against a block which is probably close to
 * this one.  So you should push what I/O you have currently accumulated.
 *
 * This all causes the disk requests to be issued in the correct order.
 */
// 总的来说就是尽可能构造（合并）最大的连续的bio
// mpage在最后会保证所有按照页面顺序加入bio的page中的block是连续的
// 该流程被ext2_readpages直接使用，但不被ext4_readpages使用（ext4自己写了一套）
// 传参部分的mapping由打开文件提供（filp->f_mapping），pages为待读页面集合，详见注释
// 那么像ext2这种a_ops直接调用mpage_readpages又该怎么和它自己具体文件系统内的数据结构交互？
// 实际上是通过do_mpage_readpage继续传递get_block完成，并且从这里会涉及到page->bio
int
mpage_readpages(struct address_space *mapping, struct list_head *pages,
				unsigned nr_pages, get_block_t get_block)
{
	struct bio *bio = NULL;
	unsigned page_idx;
	sector_t last_block_in_bio = 0;
	struct buffer_head map_bh;
	unsigned long first_logical_block = 0;
	gfp_t gfp = readahead_gfp_mask(mapping);

	map_bh.b_state = 0;
	map_bh.b_size = 0;
	for (page_idx = 0; page_idx < nr_pages; page_idx++) {
		struct page *page = lru_to_page(pages);

		prefetchw(&page->flags);
		list_del(&page->lru);
		// 这个接口命名令人有点困惑，毕竟mapping又不是lru形式，怎么就page cache lru了
		// 其实意思是：既插入page cache中，也插入lru中
		// 对应的有其他接口如add_to_page_cache_locked，就是只插入page cache不插入lru（见注释部分）
		// 查找旧的内核代码更容易看懂
		if (!add_to_page_cache_lru(page, mapping,
					page->index,
					gfp)) {
			bio = do_mpage_readpage(bio, page,
					// 假设的一种理想情况，尝试用单个bio处理掉剩下的所有pages
					// 可以对比一下mpage_readpage，对应的会设为1
					// 其实就是greedy的思路，万一成功了呢
					nr_pages - page_idx,
					&last_block_in_bio, &map_bh,
					&first_logical_block,
					get_block, gfp);
		}
		put_page(page);
	}
	// 按道理所有pages都处理了
	BUG_ON(!list_empty(pages));
	if (bio)
		mpage_bio_submit(REQ_OP_READ, 0, bio);
	return 0;
}


/*
 * This is the worker routine which does all the work of mapping the disk
 * blocks and constructs largest possible bios, submits them for IO if the
 * blocks are not contiguous on the disk.
 *
 * We pass a buffer_head back and forth and use its buffer_mapped() flag to
 * represent the validity of its disk mapping and to decide when to do the next
 * get_block() call.
 */
static struct bio *
do_mpage_readpage(struct bio *bio, struct page *page, unsigned nr_pages,
		sector_t *last_block_in_bio, struct buffer_head *map_bh,
		unsigned long *first_logical_block, get_block_t get_block,
		gfp_t gfp)
{
	struct inode *inode = page->mapping->host;
	const unsigned blkbits = inode->i_blkbits;
	// 一个页面应该存放多少个块
	const unsigned blocks_per_page = PAGE_SIZE >> blkbits;
	const unsigned blocksize = 1 << blkbits;
	sector_t block_in_file;
	sector_t last_block;
	sector_t last_block_in_file;
	// TODO 这个应该是暂存get_block得到的sector结果，但具体用途用于哪里？
	sector_t blocks[MAX_BUF_PER_PAGE];
	unsigned page_block;
	unsigned first_hole = blocks_per_page;
	struct block_device *bdev = NULL;
	int length;
	int fully_mapped = 1;
	unsigned nblocks;
	unsigned relative_block;

	// 该页是在页高速缓存上？
	if (page_has_buffers(page))
		goto confused;

	// 页中第一块的文件块号
	block_in_file = (sector_t)page->index << (PAGE_SHIFT - blkbits);
	// 要读的最后一页中最后一块的块号
	last_block = block_in_file + nr_pages * blocks_per_page;
	// 文件中的最后的块号（向上取整）
	last_block_in_file = (i_size_read(inode) + blocksize - 1) >> blkbits;
	if (last_block > last_block_in_file)
		last_block = last_block_in_file;
	page_block = 0;

	/*
	 * Map blocks using the result from the previous get_blocks call first.
	 */
	// 初次传入时，b_size和*first_logical_block为0
	nblocks = map_bh->b_size >> blkbits;
	// block_in_file的判断是说，位于first_logical_block和bh最后一个块之间
	// 初次时不应该mapped
	// 这应该是处理已经map了但是继续处理未映射部分
	// 总之先忽略吧
	if (buffer_mapped(map_bh) && block_in_file > *first_logical_block &&
			block_in_file < (*first_logical_block + nblocks)) {
		unsigned map_offset = block_in_file - *first_logical_block;
		unsigned last = nblocks - map_offset;

		for (relative_block = 0; ; relative_block++) {
			if (relative_block == last) {
				clear_buffer_mapped(map_bh);
				break;
			}
			if (page_block == blocks_per_page)
				break;
			blocks[page_block] = map_bh->b_blocknr + map_offset +
						relative_block;
			page_block++;
			block_in_file++;
		}
		bdev = map_bh->b_bdev;
	}

	/*
	 * Then do more get_blocks calls until we are done with this page.
	 */
	map_bh->b_page = page;
	// 对这个page中的block处理
	while (page_block < blocks_per_page) {
		// mapped状态也在这个b_state里
		map_bh->b_state = 0;
		map_bh->b_size = 0;

		if (block_in_file < last_block) {
			// 按字节算
			// 使用get_block前需要告知b_size
			map_bh->b_size = (last_block-block_in_file) << blkbits;
			// block_in_file告知起始，b_size告知大小
			// 内部实现认为此处操作没问题的话，get_block接口会干这些收尾事情
			// 1. 执行map_bh，
			// 2. 设置实际的b_size
			// 3. 也会告知new / boundary等提示
			if (get_block(inode, block_in_file, map_bh, 0))
				// 不连续？
				goto confused;
			*first_logical_block = block_in_file;
		}

		// TODO hole
		if (!buffer_mapped(map_bh)) {
			fully_mapped = 0;
			if (first_hole == blocks_per_page)
				first_hole = page_block;
			page_block++;
			block_in_file++;
			continue;
		}

		/* some filesystems will copy data into the page during
		 * the get_block call, in which case we don't want to
		 * read it again.  map_buffer_to_page copies the data
		 * we just collected from get_block into the page's buffers
		 * so readpage doesn't have to repeat the get_block call
		 */
		// 如果bh最新了，那就copy到page
		if (buffer_uptodate(map_bh)) {
			map_buffer_to_page(page, map_bh, page_block);
			goto confused;
		}
	
		// Question. 有文件空洞则无法读整页？什么情况下出现？
		if (first_hole != blocks_per_page)
			goto confused;		/* hole -> non-hole */

		/* Contiguous blocks? */
		// 应该是不连续？上一个blocks对不上bh预想的号
		// TODO map_bh->b_blocknr应该是从get_block ----> map_bh ----> bno ----> Indirect.key 构造而来的（ext2流程下）
		// 看样子是对单个block进行get_block后可以对页内直接处理（除了confused）
		if (page_block && blocks[page_block-1] != map_bh->b_blocknr-1)
			goto confused;
		nblocks = map_bh->b_size >> blkbits;
		for (relative_block = 0; ; relative_block++) {
			if (relative_block == nblocks) {
				clear_buffer_mapped(map_bh);
				break;
			} else if (page_block == blocks_per_page)
				break;
			blocks[page_block] = map_bh->b_blocknr+relative_block;
			page_block++;
			block_in_file++;
		}
		bdev = map_bh->b_bdev;
	}

	// 至此确保页中所有的块在磁盘上是相邻的
	// 可能最后一页不是完整的，没有映射，需要在对应page缓冲填0
	if (first_hole != blocks_per_page) {
		// 由于空洞是随机数据，因此需要清0
		zero_user_segment(page, first_hole << blkbits, PAGE_SIZE);
		if (first_hole == 0) {
			SetPageUptodate(page);
			unlock_page(page);
			goto out;
		}
	} else if (fully_mapped) {
		SetPageMappedToDisk(page);
	}

	if (fully_mapped && blocks_per_page == 1 && !PageUptodate(page) &&
	    cleancache_get_page(page) == 0) {
		SetPageUptodate(page);
		goto confused;
	}

	/*
	 * This page will go to BIO.  Do we need to send this BIO off first?
	 */
	// 发生了跨页？last_block_in_bio可以想象为。。。blocks[-1]？
	// 此时的bio还是指代上一次的mpage结果？
	if (bio && (*last_block_in_bio != blocks[0] - 1))
		bio = mpage_bio_submit(REQ_OP_READ, 0, bio);

alloc_new:
	if (bio == NULL) {
		if (first_hole == blocks_per_page) {
			if (!bdev_read_page(bdev, blocks[0] << (blkbits - 9),
								page))
				goto out;
		}
		bio = mpage_alloc(bdev, blocks[0] << (blkbits - 9),
				min_t(int, nr_pages, BIO_MAX_PAGES), gfp);
		if (bio == NULL)
			goto confused;
	}

	length = first_hole << blkbits;
	if (bio_add_page(bio, page, length, 0) < length) {
		bio = mpage_bio_submit(REQ_OP_READ, 0, bio);
		goto alloc_new;
	}

	relative_block = block_in_file - *first_logical_block;
	nblocks = map_bh->b_size >> blkbits;
	if ((buffer_boundary(map_bh) && relative_block == nblocks) ||
	    (first_hole != blocks_per_page))
		// 提交bio后将返回NULL
		bio = mpage_bio_submit(REQ_OP_READ, 0, bio);
	else
		*last_block_in_bio = blocks[blocks_per_page - 1];
out:
	return bio;

confused:
	if (bio)
		bio = mpage_bio_submit(REQ_OP_READ, 0, bio);
	if (!PageUptodate(page))
	        block_read_full_page(page, get_block);
	else
		unlock_page(page);
	goto out;
}







static struct bio *mpage_bio_submit(int op, int op_flags, struct bio *bio)
{
	bio->bi_end_io = mpage_end_io;
	bio_set_op_attrs(bio, op, op_flags);
	guard_bio_eod(op, bio);
	submit_bio(bio);
	return NULL;
}



// 文件：/block/blk-core.c

/**
 * submit_bio - submit a bio to the block device layer for I/O
 * @bio: The &struct bio which describes the I/O
 *
 * submit_bio() is very similar in purpose to generic_make_request(), and
 * uses that function to do most of the work. Both are fairly rough
 * interfaces; @bio must be presetup and ready for I/O.
 *
 */
blk_qc_t submit_bio(struct bio *bio)
{
	/*
	 * If it's a regular read/write or a barrier with data attached,
	 * go through the normal accounting stuff before submission.
	 */
	if (bio_has_data(bio)) {
		unsigned int count;

		if (unlikely(bio_op(bio) == REQ_OP_WRITE_SAME))
			count = queue_logical_block_size(bio->bi_disk->queue) >> 9;
		else
			count = bio_sectors(bio);

		if (op_is_write(bio_op(bio))) {
			count_vm_events(PGPGOUT, count);
		} else {
			task_io_account_read(bio->bi_iter.bi_size);
			count_vm_events(PGPGIN, count);
		}

		if (unlikely(block_dump)) {
			char b[BDEVNAME_SIZE];
			printk(KERN_DEBUG "%s(%d): %s block %Lu on %s (%u sectors)\n",
			current->comm, task_pid_nr(current),
				op_is_write(bio_op(bio)) ? "WRITE" : "READ",
				(unsigned long long)bio->bi_iter.bi_sector,
				bio_devname(bio, b), count);
		}
	}

	return generic_make_request(bio);
}



/**
 * generic_make_request - hand a buffer to its device driver for I/O
 * @bio:  The bio describing the location in memory and on the device.
 *
 * generic_make_request() is used to make I/O requests of block
 * devices. It is passed a &struct bio, which describes the I/O that needs
 * to be done.
 *
 * generic_make_request() does not return any status.  The
 * success/failure status of the request, along with notification of
 * completion, is delivered asynchronously through the bio->bi_end_io
 * function described (one day) else where.
 *
 * The caller of generic_make_request must make sure that bi_io_vec
 * are set to describe the memory buffer, and that bi_dev and bi_sector are
 * set to describe the device address, and the
 * bi_end_io and optionally bi_private are set to describe how
 * completion notification should be signaled.
 *
 * generic_make_request and the drivers it calls may use bi_next if this
 * bio happens to be merged with someone else, and may resubmit the bio to
 * a lower device by calling into generic_make_request recursively, which
 * means the bio should NOT be touched after the call to ->make_request_fn.
 */
blk_qc_t generic_make_request(struct bio *bio)
{
	/*
	 * bio_list_on_stack[0] contains bios submitted by the current
	 * make_request_fn.
	 * bio_list_on_stack[1] contains bios that were submitted before
	 * the current make_request_fn, but that haven't been processed
	 * yet.
	 */
	struct bio_list bio_list_on_stack[2];
	blk_mq_req_flags_t flags = 0;
	struct request_queue *q = bio->bi_disk->queue;
	blk_qc_t ret = BLK_QC_T_NONE;

	if (bio->bi_opf & REQ_NOWAIT)
		flags = BLK_MQ_REQ_NOWAIT;
	if (bio_flagged(bio, BIO_QUEUE_ENTERED))
		blk_queue_enter_live(q);
	else if (blk_queue_enter(q, flags) < 0) {
		if (!blk_queue_dying(q) && (bio->bi_opf & REQ_NOWAIT))
			bio_wouldblock_error(bio);
		else
			bio_io_error(bio);
		return ret;
	}

	if (!generic_make_request_checks(bio))
		goto out;

	/*
	 * We only want one ->make_request_fn to be active at a time, else
	 * stack usage with stacked devices could be a problem.  So use
	 * current->bio_list to keep a list of requests submited by a
	 * make_request_fn function.  current->bio_list is also used as a
	 * flag to say if generic_make_request is currently active in this
	 * task or not.  If it is NULL, then no make_request is active.  If
	 * it is non-NULL, then a make_request is active, and new requests
	 * should be added at the tail
	 */
	if (current->bio_list) {
		bio_list_add(&current->bio_list[0], bio);
		goto out;
	}

	/* following loop may be a bit non-obvious, and so deserves some
	 * explanation.
	 * Before entering the loop, bio->bi_next is NULL (as all callers
	 * ensure that) so we have a list with a single bio.
	 * We pretend that we have just taken it off a longer list, so
	 * we assign bio_list to a pointer to the bio_list_on_stack,
	 * thus initialising the bio_list of new bios to be
	 * added.  ->make_request() may indeed add some more bios
	 * through a recursive call to generic_make_request.  If it
	 * did, we find a non-NULL value in bio_list and re-enter the loop
	 * from the top.  In this case we really did just take the bio
	 * of the top of the list (no pretending) and so remove it from
	 * bio_list, and call into ->make_request() again.
	 */
	BUG_ON(bio->bi_next);
	bio_list_init(&bio_list_on_stack[0]);
	current->bio_list = bio_list_on_stack;
	do {
		bool enter_succeeded = true;

		if (unlikely(q != bio->bi_disk->queue)) {
			if (q)
				blk_queue_exit(q);
			q = bio->bi_disk->queue;
			flags = 0;
			if (bio->bi_opf & REQ_NOWAIT)
				flags = BLK_MQ_REQ_NOWAIT;
			if (blk_queue_enter(q, flags) < 0) {
				enter_succeeded = false;
				q = NULL;
			}
		}

		if (enter_succeeded) {
			struct bio_list lower, same;

			/* Create a fresh bio_list for all subordinate requests */
			bio_list_on_stack[1] = bio_list_on_stack[0];
			bio_list_init(&bio_list_on_stack[0]);
			ret = q->make_request_fn(q, bio);

			/* sort new bios into those for a lower level
			 * and those for the same level
			 */
			bio_list_init(&lower);
			bio_list_init(&same);
			while ((bio = bio_list_pop(&bio_list_on_stack[0])) != NULL)
				if (q == bio->bi_disk->queue)
					bio_list_add(&same, bio);
				else
					bio_list_add(&lower, bio);
			/* now assemble so we handle the lowest level first */
			bio_list_merge(&bio_list_on_stack[0], &lower);
			bio_list_merge(&bio_list_on_stack[0], &same);
			bio_list_merge(&bio_list_on_stack[0], &bio_list_on_stack[1]);
		} else {
			if (unlikely(!blk_queue_dying(q) &&
					(bio->bi_opf & REQ_NOWAIT)))
				bio_wouldblock_error(bio);
			else
				bio_io_error(bio);
		}
		bio = bio_list_pop(&bio_list_on_stack[0]);
	} while (bio);
	current->bio_list = NULL; /* deactivate */

out:
	if (q)
		blk_queue_exit(q);
	return ret;
}