// 文件：/fs/buffer.c

/*
 * block_write_begin takes care of the basic task of block allocation and
 * bringing partial write blocks uptodate first.
 *
 * The filesystem needs to handle block truncation upon failure.
 */
int block_write_begin(struct address_space *mapping, loff_t pos, unsigned len,
		unsigned flags, struct page **pagep, get_block_t *get_block)
{
	pgoff_t index = pos >> PAGE_SHIFT;
	struct page *page;
	int status;

	// 分配缓存页
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;

	status = __block_write_begin(page, pos, len, get_block);
	if (unlikely(status)) {
		unlock_page(page);
		put_page(page);
		page = NULL;
	}

	*pagep = page;
	return status;
}

int __block_write_begin(struct page *page, loff_t pos, unsigned len,
		get_block_t *get_block)
{
	return __block_write_begin_int(page, pos, len, get_block, NULL);
}

int __block_write_begin_int(struct page *page, loff_t pos, unsigned len,
		get_block_t *get_block, struct iomap *iomap)
{
	unsigned from = pos & (PAGE_SIZE - 1);
	unsigned to = from + len;
	struct inode *inode = page->mapping->host;
	unsigned block_start, block_end;
	sector_t block;
	int err = 0;
	unsigned blocksize, bbits;
	struct buffer_head *bh, *head, *wait[2], **wait_bh=wait;

	BUG_ON(!PageLocked(page));
	BUG_ON(from > PAGE_SIZE);
	BUG_ON(to > PAGE_SIZE);
	BUG_ON(from > to);

	head = create_page_buffers(page, inode, 0);
	blocksize = head->b_size;
	bbits = block_size_bits(blocksize);

	block = (sector_t)page->index << (PAGE_SHIFT - bbits);

	for(bh = head, block_start = 0; bh != head || !block_start;
	    block++, block_start=block_end, bh = bh->b_this_page) {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (PageUptodate(page)) {
				if (!buffer_uptodate(bh))
					set_buffer_uptodate(bh);
			}
			continue;
		}
		if (buffer_new(bh))
			clear_buffer_new(bh);
		if (!buffer_mapped(bh)) {
			WARN_ON(bh->b_size != blocksize);
			if (get_block) {
				err = get_block(inode, block, bh, 1);
				if (err)
					break;
			} else {
				iomap_to_bh(inode, block, bh, iomap);
			}

			if (buffer_new(bh)) {
				clean_bdev_bh_alias(bh);
				if (PageUptodate(page)) {
					clear_buffer_new(bh);
					set_buffer_uptodate(bh);
					mark_buffer_dirty(bh);
					continue;
				}
				if (block_end > to || block_start < from)
					zero_user_segments(page,
						to, block_end,
						block_start, from);
				continue;
			}
		}
		if (PageUptodate(page)) {
			if (!buffer_uptodate(bh))
				set_buffer_uptodate(bh);
			continue; 
		}
		if (!buffer_uptodate(bh) && !buffer_delay(bh) &&
		    !buffer_unwritten(bh) &&
		     (block_start < from || block_end > to)) {
			ll_rw_block(REQ_OP_READ, 0, 1, &bh);
			*wait_bh++=bh;
		}
	}
	/*
	 * If we issued read requests - let them complete.
	 */
	while(wait_bh > wait) {
		wait_on_buffer(*--wait_bh);
		if (!buffer_uptodate(*wait_bh))
			err = -EIO;
	}
	if (unlikely(err))
		page_zero_new_buffers(page, from, to);
	return err;
}