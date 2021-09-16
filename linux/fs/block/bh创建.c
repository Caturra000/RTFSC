// 文件：/fs/buffer.c

static struct buffer_head *create_page_buffers(struct page *page, struct inode *inode, unsigned int b_state)
{
	BUG_ON(!PageLocked(page));

	// helper function #1
	if (!page_has_buffers(page))
		create_empty_buffers(page, 1 << READ_ONCE(inode->i_blkbits),
				     b_state);
	// helper function #2
	return page_buffers(page);
}

/*
 * We attach and possibly dirty the buffers atomically wrt
 * __set_page_dirty_buffers() via private_lock.  try_to_free_buffers
 * is already excluded via the page lock.
 */
void create_empty_buffers(struct page *page,
			unsigned long blocksize, unsigned long b_state)
{
	struct buffer_head *bh, *head, *tail;

	head = alloc_page_buffers(page, blocksize, true);
	bh = head;
	do {
		bh->b_state |= b_state;
		tail = bh;
		bh = bh->b_this_page;
	} while (bh);
	tail->b_this_page = head;

	spin_lock(&page->mapping->private_lock);
	if (PageUptodate(page) || PageDirty(page)) {
		bh = head;
		do {
			if (PageDirty(page))
				set_buffer_dirty(bh);
			if (PageUptodate(page))
				set_buffer_uptodate(bh);
			bh = bh->b_this_page;
		} while (bh != head);
	}
	// helper function #3
	attach_page_buffers(page, head);
	spin_unlock(&page->mapping->private_lock);
}

/*
 * Create the appropriate buffers when given a page for data area and
 * the size of each buffer.. Use the bh->b_this_page linked list to
 * follow the buffers created.  Return NULL if unable to create more
 * buffers.
 *
 * The retry flag is used to differentiate async IO (paging, swapping)
 * which may not fail from ordinary buffer allocations.
 */
struct buffer_head *alloc_page_buffers(struct page *page, unsigned long size,
		bool retry)
{
	struct buffer_head *bh, *head;
	gfp_t gfp = GFP_NOFS;
	long offset;

	if (retry)
		gfp |= __GFP_NOFAIL;

	head = NULL;
	offset = PAGE_SIZE;
	while ((offset -= size) >= 0) {
		bh = alloc_buffer_head(gfp);
		if (!bh)
			goto no_grow;

		bh->b_this_page = head;
		bh->b_blocknr = -1;
		head = bh;

		bh->b_size = size;

		/* Link the buffer to its page */
		set_bh_page(bh, page, offset);
	}
	return head;
/*
 * In case anything failed, we just free everything we got.
 */
no_grow:
	if (head) {
		do {
			bh = head;
			head = head->b_this_page;
			free_buffer_head(bh);
		} while (head);
	}

	return NULL;
}


/// helper function

// helper function #1
#define page_has_buffers(page)	PagePrivate(page)

// helper function #2
/* If we *know* page->private refers to buffer_heads */
#define page_buffers(page)					\
	({							\
		BUG_ON(!PagePrivate(page));			\
		((struct buffer_head *)page_private(page));	\
	})

// helper function #3
static inline void attach_page_buffers(struct page *page,
		struct buffer_head *head)
{
	get_page(page);
	SetPagePrivate(page);
	set_page_private(page, (unsigned long)head);
}
