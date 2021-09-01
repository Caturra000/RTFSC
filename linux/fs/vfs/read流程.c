// 文件： /fs/read_write.c

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
	return ksys_read(fd, buf, count);
}

// 1. fdget
// 2. file_pos_read 获取pos
// 3. vfs_read
// 4. file_pos_write 更新pos
// 5. fdput
ssize_t ksys_read(unsigned int fd, char __user *buf, size_t count)
{
	// 获取fd struct，从而得到f.file（并可能增加refcount）
	struct fd f = fdget_pos(fd);
	ssize_t ret = -EBADF;

	if (f.file) {
		// 当前的pos
		loff_t pos = file_pos_read(f.file); // return file->f_pos;
		ret = vfs_read(f.file, buf, count, &pos);
		// 有读入，更新pos
		if (ret >= 0)
			file_pos_write(f.file, pos); // f.file->f_ops = pos;
		// 减少refcount
		fdput_pos(f);
	}
	return ret;
}


// 一些权限的check
// 关键流程__vfs_read
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;
	if (!(file->f_mode & FMODE_CAN_READ))
		return -EINVAL;
	if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
		return -EFAULT;
	// 检测文件部分是否有冲突的强制锁
	ret = rw_verify_area(READ, file, pos, count);
	if (!ret) {
		if (count > MAX_RW_COUNT)
			count =  MAX_RW_COUNT;
		ret = __vfs_read(file, buf, count, pos);
		if (ret > 0) {
			fsnotify_access(file);
			// CONFIG_TASK_XACCT相关，略
			add_rchar(current, ret);
		}
		inc_syscr(current);
	}

	return ret;
}

// 调用f_op
ssize_t __vfs_read(struct file *file, char __user *buf, size_t count,
		   loff_t *pos)
{
	if (file->f_op->read)
		return file->f_op->read(file, buf, count, pos);
	else if (file->f_op->read_iter)
		// new_sync_read中会调用到f_op->read_iter
		return new_sync_read(file, buf, count, pos);
	else
		return -EINVAL;
}


// 参考ext2，f_op->read_iter(没有f_op->read)会调用到generic_file_read_iter

// 1. 初始化iov
// 2. call_read_iter
// 3. 更新file->f_pos(ppos)
static ssize_t new_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	// 构造用于传递数据的iov
	struct iovec iov = { .iov_base = buf, .iov_len = len };
	// 用于跟踪IO操作的完成状态
	struct kiocb kiocb;
	// 见generic_file_read_iter注释，destination for the data read
	struct iov_iter iter;
	ssize_t ret;

	init_sync_kiocb(&kiocb, filp);
	kiocb.ki_pos = *ppos;
	iov_iter_init(&iter, READ, &iov, 1, len);

	ret = call_read_iter(filp, &kiocb, &iter);
	BUG_ON(ret == -EIOCBQUEUED);
	// Question. 这个在ksys_read()是不是重复更新了？
	*ppos = kiocb.ki_pos;
	return ret;
}

// TODO https://lwn.net/Articles/625077/

static inline ssize_t call_read_iter(struct file *file, struct kiocb *kio,
				     struct iov_iter *iter)
{
	return file->f_op->read_iter(kio, iter);
}


/**
 * generic_file_read_iter - generic filesystem read routine
 * @iocb:	kernel I/O control block
 * @iter:	destination for the data read
 *
 * This is the "read_iter()" routine for all filesystems
 * that can use the page cache directly.
 */
ssize_t
generic_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	size_t count = iov_iter_count(iter); // return iter->count 需要读取的字节数
	ssize_t retval = 0;

	if (!count)
		goto out; /* skip atime */
	// TODO
	if (iocb->ki_flags & IOCB_DIRECT) {
		struct file *file = iocb->ki_filp;
		struct address_space *mapping = file->f_mapping;
		struct inode *inode = mapping->host;
		loff_t size;

		size = i_size_read(inode);
		if (iocb->ki_flags & IOCB_NOWAIT) {
			if (filemap_range_has_page(mapping, iocb->ki_pos,
						   iocb->ki_pos + count - 1))
				return -EAGAIN;
		} else {
			retval = filemap_write_and_wait_range(mapping,
						iocb->ki_pos,
					        iocb->ki_pos + count - 1);
			if (retval < 0)
				goto out;
		}

		file_accessed(file);

		retval = mapping->a_ops->direct_IO(iocb, iter);
		if (retval >= 0) {
			iocb->ki_pos += retval;
			count -= retval;
		}
		iov_iter_revert(iter, count - iov_iter_count(iter));

		/*
		 * Btrfs can have a short DIO read if we encounter
		 * compressed extents, so if there was an error, or if
		 * we've already read everything we wanted to, or if
		 * there was a short read because we hit EOF, go ahead
		 * and return.  Otherwise fallthrough to buffered io for
		 * the rest of the read.  Buffered reads will not work for
		 * DAX files, so don't bother trying.
		 */
		if (retval < 0 || !count || iocb->ki_pos >= size ||
		    IS_DAX(inode))
			goto out;
	}
	// 经过page cache
	retval = generic_file_buffered_read(iocb, iter, retval);
out:
	return retval;
}


/**
 * generic_file_buffered_read - generic file read routine
 * @iocb:	the iocb to read
 * @iter:	data destination
 * @written:	already copied
 *
 * This is a generic file read routine, and uses the
 * mapping->a_ops->readpage() function for the actual low-level stuff.
 *
 * This is really ugly. But the goto's actually try to clarify some
 * of the logic when it comes to error handling etc.
 */
// 真正的读内容的流程在mapping->a_ops->readpage()，暂不考虑
// 这里描述一个通用的buffered读流程
// 虽然代码很ugly（不是我说的哈，注释写的），但思路可以看出就是
// 1. 获取之前的read ahead信息
// 2. find page (read page || get cached-page)
// 3. cache this page
// 4. check page whether up-to-date
// 5. copy to user
// 6. loop, goto step 1
// 其中，read page需要mapping实际读入流程的参与
// 不考虑error handling还是可以看得下的
static ssize_t generic_file_buffered_read(struct kiocb *iocb,
		struct iov_iter *iter, ssize_t written) // 一般来说，written传入为0
{
	struct file *filp = iocb->ki_filp;
	// 获取address_space
	struct address_space *mapping = filp->f_mapping;
	// mapping归属的inode
	struct inode *inode = mapping->host;
	// read ahead标志
	// 对于一个刚构造的ra，默认是全0，除了prev_pos为-1，ra_pages看inode
	struct file_ra_state *ra = &filp->f_ra;
	loff_t *ppos = &iocb->ki_pos;
	pgoff_t index;
	pgoff_t last_index;
	pgoff_t prev_index;
	unsigned long offset;      /* offset into pagecache page */
	unsigned int prev_offset;
	int error = 0;

	if (unlikely(*ppos >= inode->i_sb->s_maxbytes))
		return 0;
	iov_iter_truncate(iter, inode->i_sb->s_maxbytes);
	// 代码检索比较难找PAGE_SHIFT在特定体系下的大小，目前只考虑4K页面（大小为12）
	// index + offset可以用于find page
	// prev_index + prevoffset暂未看具体流程，但直觉告诉我这是已经read ahead的index + offset，那么中间做差可以获得cache
	index = *ppos >> PAGE_SHIFT;
	prev_index = ra->prev_pos >> PAGE_SHIFT;
	prev_offset = ra->prev_pos & (PAGE_SIZE-1);
	// 读请求是按page算的，因此需要对*ppos + iter->count加上PAGE_SIZE-1进行向上取整
	last_index = (*ppos + iter->count + PAGE_SIZE-1) >> PAGE_SHIFT;
	offset = *ppos & ~PAGE_MASK;

	for (;;) {
		struct page *page;
		pgoff_t end_index;
		loff_t isize;
		unsigned long nr, ret;

		cond_resched();
// 注意不管是find_page还是readpage流程都有a_ops参与，只是该流程会尽可能减少/延迟a_ops的操作
find_page:
		if (fatal_signal_pending(current)) {
			error = -EINTR;
			goto out;
		}
		// 在radix tree中查找page，index对应于page页面号
		// 判断是否已存在
		page = find_get_page(mapping, index);
		if (!page) {
			if (iocb->ki_flags & IOCB_NOWAIT)
				goto would_block;
			// 预读
			// 该函数只有在cache miss时才允许调用，一般最大会预读2MB大小
			// 虽然函数的req_size参数设为last_index - index，企图一次性readahead全部，但内部依然会使用限制因子ra->ra_pages进行截断
			page_cache_sync_readahead(mapping,
					ra, filp,
					index, last_index - index);
			page = find_get_page(mapping, index);
			if (unlikely(page == NULL))
				// 没有多余的page cache
				goto no_cached_page;
		}
		// TODO 这些大写开头的无法找到define
		// ref: https://unix.stackexchange.com/questions/381983/where-is-pagewriteback-defined-in-the-4-9-linux-kernel-source
		// 使用宇宙第一IDE可以解决这个问题
		// PS. 这里的flag是复用的，原因是page的flag占位过于珍贵，但不使用flag又不能精确判断
		// 当前面完成了预读时，用户请求后面的一个page会打上readahead标记
		// 因此下一次读到已经cache的page会触发该async流程
		if (PageReadahead(page)) {
			// Question. 代码似乎与sync_readahead流程差不多，怎么体现出async？
			// Question. 有不少判断不适合的条件就直接return了，这样是不是没有async请求了？
			page_cache_async_readahead(mapping,
					ra, filp, page,
					index, last_index - index);
		}
		// page已经cache != page是最新的
		if (!PageUptodate(page)) {
			if (iocb->ki_flags & IOCB_NOWAIT) {
				put_page(page);
				// 提示EAGAIN
				goto would_block;
			}

			/*
			 * See comment in do_read_cache_page on why
			 * wait_on_page_locked is used to avoid unnecessarily
			 * serialisations and why it's safe.
			 */
			error = wait_on_page_locked_killable(page);
			if (unlikely(error))
				goto readpage_error;
			if (PageUptodate(page))
				goto page_ok;

			if (inode->i_blkbits == PAGE_SHIFT ||
					!mapping->a_ops->is_partially_uptodate)
				goto page_not_up_to_date;
			/* pipes can't handle partially uptodate pages */
			if (unlikely(iter->type & ITER_PIPE))
				goto page_not_up_to_date;
			if (!trylock_page(page))
				goto page_not_up_to_date;
			/* Did it get truncated before we got the lock? */
			if (!page->mapping)
				goto page_not_up_to_date_locked;
			if (!mapping->a_ops->is_partially_uptodate(page,
							offset, iter->count))
				goto page_not_up_to_date_locked;
			unlock_page(page);
		}
page_ok:
		/*
		 * i_size must be checked after we know the page is Uptodate.
		 *
		 * Checking i_size after the check allows us to calculate
		 * the correct value for "nr", which means the zero-filled
		 * part of the page is not copied back to userspace (unless
		 * another truncate extends the file - this is desired though).
		 */

		isize = i_size_read(inode); // 返回inode->i_size
		end_index = (isize - 1) >> PAGE_SHIFT;
		if (unlikely(!isize || index > end_index)) {
			put_page(page);
			goto out;
		}

		/* nr is the maximum number of bytes to copy from this page */
		nr = PAGE_SIZE;
		if (index == end_index) {
			nr = ((isize - 1) & ~PAGE_MASK) + 1;
			if (nr <= offset) {
				put_page(page);
				goto out;
			}
		}
		nr = nr - offset;

		/* If users can be writing to this page using arbitrary
		 * virtual addresses, take care about potential aliasing
		 * before reading the page on the kernel side.
		 */
		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		/*
		 * When a sequential read accesses a page several times,
		 * only mark it as accessed the first time.
		 */
		if (prev_index != index || offset != prev_offset)
			mark_page_accessed(page);
		prev_index = index;

		/*
		 * Ok, we have the page, and it's up-to-date, so
		 * now we can copy it to user space...
		 */

		ret = copy_page_to_iter(page, offset, nr, iter);
		offset += ret;
		index += offset >> PAGE_SHIFT;
		offset &= ~PAGE_MASK;
		prev_offset = offset;

		put_page(page);
		written += ret;
		if (!iov_iter_count(iter))
			goto out;
		if (ret < nr) {
			error = -EFAULT;
			goto out;
		}
		continue;

page_not_up_to_date:
		/* Get exclusive access to the page ... */
		error = lock_page_killable(page);
		if (unlikely(error))
			goto readpage_error;

page_not_up_to_date_locked:
		/* Did it get truncated before we got the lock? */
		if (!page->mapping) {
			unlock_page(page);
			put_page(page);
			continue;
		}

		/* Did somebody else fill it already? */
		if (PageUptodate(page)) {
			unlock_page(page);
			goto page_ok;
		}

readpage:
		/*
		 * A previous I/O error may have been due to temporary
		 * failures, eg. multipath errors.
		 * PG_error will be set again if readpage fails.
		 */
		ClearPageError(page);
		/* Start the actual read. The read will unlock the page. */
		error = mapping->a_ops->readpage(filp, page); // 以ext2为例，ext2_readpage

		if (unlikely(error)) {
			if (error == AOP_TRUNCATED_PAGE) {
				put_page(page);
				error = 0;
				goto find_page;
			}
			goto readpage_error;
		}

		if (!PageUptodate(page)) {
			error = lock_page_killable(page);
			if (unlikely(error))
				goto readpage_error;
			if (!PageUptodate(page)) {
				if (page->mapping == NULL) {
					/*
					 * invalidate_mapping_pages got it
					 */
					unlock_page(page);
					put_page(page);
					goto find_page;
				}
				unlock_page(page);
				shrink_readahead_size_eio(filp, ra);
				error = -EIO;
				goto readpage_error;
			}
			unlock_page(page);
		}

		goto page_ok;

readpage_error:
		/* UHHUH! A synchronous read error occurred. Report it */
		put_page(page);
		goto out;

no_cached_page:
		/*
		 * Ok, it wasn't cached, so we need to create a new
		 * page..
		 */
		page = page_cache_alloc(mapping);
		if (!page) {
			error = -ENOMEM;
			goto out;
		}
		error = add_to_page_cache_lru(page, mapping, index,
				mapping_gfp_constraint(mapping, GFP_KERNEL));
		if (error) {
			put_page(page);
			if (error == -EEXIST) {
				error = 0;
				goto find_page;
			}
			goto out;
		}
		goto readpage;
	}

would_block:
	error = -EAGAIN;
out:
	ra->prev_pos = prev_index;
	ra->prev_pos <<= PAGE_SHIFT;
	ra->prev_pos |= prev_offset;

	*ppos = ((loff_t)index << PAGE_SHIFT) + offset;
	file_accessed(filp);
	return written ? written : error;
}

/**
 * page_cache_sync_readahead - generic file readahead
 * @mapping: address_space which holds the pagecache and I/O vectors
 * @ra: file_ra_state which holds the readahead state
 * @filp: passed on to ->readpage() and ->readpages()
 * @offset: start offset into @mapping, in pagecache page-sized units
 * @req_size: hint: total size of the read which the caller is performing in
 *            pagecache pages
 *
 * page_cache_sync_readahead() should be called when a cache miss happened:
 * it will submit the read.  The readahead logic may decide to piggyback more
 * pages onto the read request if access patterns suggest it will improve
 * performance.
 */
void page_cache_sync_readahead(struct address_space *mapping,
			       struct file_ra_state *ra, struct file *filp,
			       pgoff_t offset, unsigned long req_size)
{
	/* no read-ahead */
	if (!ra->ra_pages)
		return;

	/* be dumb */
	// TODO 这个分支需要进一步了解posix_fadvise
	// posix_fadvise() https://www.yuanguohuo.com/2020/03/24/linux-read-ahead/
	// 这个FMODE到底在哪里有使用到 https://elixir.bootlin.com/linux/v4.18.20/C/ident/FMODE_RANDOM
	if (filp && (filp->f_mode & FMODE_RANDOM)) {
		force_page_cache_readahead(mapping, filp, offset, req_size);
		return;
	}

	/* do read-ahead */
	ondemand_readahead(mapping, ra, filp, false, offset, req_size);
}

/**
 * page_cache_async_readahead - file readahead for marked pages
 * @mapping: address_space which holds the pagecache and I/O vectors
 * @ra: file_ra_state which holds the readahead state
 * @filp: passed on to ->readpage() and ->readpages()
 * @page: the page at @offset which has the PG_readahead flag set
 * @offset: start offset into @mapping, in pagecache page-sized units
 * @req_size: hint: total size of the read which the caller is performing in
 *            pagecache pages
 *
 * page_cache_async_readahead() should be called when a page is used which
 * has the PG_readahead flag; this is a marker to suggest that the application
 * has used up enough of the readahead window that we should start pulling in
 * more pages.
 */
void
page_cache_async_readahead(struct address_space *mapping,
			   struct file_ra_state *ra, struct file *filp,
			   struct page *page, pgoff_t offset,
			   unsigned long req_size)
{
	/* no read-ahead */
	if (!ra->ra_pages)
		return;

	/*
	 * Same bit is used for PG_readahead and PG_reclaim.
	 */
	// 正在writeback？
	if (PageWriteback(page))
		return;

	ClearPageReadahead(page);

	/*
	 * Defer asynchronous read-ahead on IO congestion.
	 */
	// TODO backing device
	if (inode_read_congested(mapping->host))
		return;

	// 推测前两个条件return是因为设备是处于占用的状态，就没法异步了
	// 但是这样是否下一次迭代就不会再次遇到这个readahead flag的page？

	/* do read-ahead */
	ondemand_readahead(mapping, ra, filp, true, offset, req_size);
}

// 关于ondemand_readahead可以见/mm/readahead.c的注释说明
// 也可以见下方链接
// https://lwn.net/Articles/235181/

/*
 * On-demand readahead design.
 *
 * The fields in struct file_ra_state represent the most-recently-executed
 * readahead attempt:
 *
 *                        |<----- async_size ---------|
 *     |------------------- size -------------------->|
 *     |==================#===========================|
 *     ^start             ^page marked with PG_readahead
 *
 * To overlap application thinking time and disk I/O time, we do
 * `readahead pipelining': Do not wait until the application consumed all
 * readahead pages and stalled on the missing page at readahead_index;
 * Instead, submit an asynchronous readahead I/O as soon as there are
 * only async_size pages left in the readahead window. Normally async_size
 * will be equal to size, for maximum pipelining.
 *
 * In interleaved sequential reads, concurrent streams on the same fd can
 * be invalidating each other's readahead state. So we flag the new readahead
 * page at (start+size-async_size) with PG_readahead, and use it as readahead
 * indicator. The flag won't be set on already cached pages, to avoid the
 * readahead-for-nothing fuss, saving pointless page cache lookups.
 *
 * prev_pos tracks the last visited byte in the _previous_ read request.
 * It should be maintained by the caller, and will be used for detecting
 * small random reads. Note that the readahead algorithm checks loosely
 * for sequential patterns. Hence interleaved reads might be served as
 * sequential ones.
 *
 * There is a special-case: if the first page which the application tries to
 * read happens to be the first page of the file, it is assumed that a linear
 * read is about to happen and the window is immediately set to the initial size
 * based on I/O request size and the max_readahead.
 *
 * The code ramps up the readahead size aggressively at first, but slow down as
 * it approaches max_readhead.
 */

/*
 * A minimal readahead algorithm for trivial sequential/random reads.
 */
static unsigned long
ondemand_readahead(struct address_space *mapping,
		   struct file_ra_state *ra, struct file *filp,
		   bool hit_readahead_marker, pgoff_t offset,
		   unsigned long req_size)
{
	struct backing_dev_info *bdi = inode_to_bdi(mapping->host);
	// 可以认为一般是32 PAGES
	unsigned long max_pages = ra->ra_pages;
	unsigned long add_pages;
	pgoff_t prev_offset;

	/*
	 * If the request exceeds the readahead window, allow the read to
	 * be up to the optimal hardware IO size
	 */
	// io_pages是bdi的最大允许IO大小
	if (req_size > max_pages && bdi->io_pages > max_pages)
		max_pages = min(req_size, bdi->io_pages);

	/*
	 * start of file
	 */
	// 如果是第一次读/或者读首部，那直接走初始化ra窗口的流程
	if (!offset)
		goto initial_readahead;

	/*
	 * It's the expected callback offset, assume sequential access.
	 * Ramp up sizes, and push forward the readahead window.
	 */
	// 针对sequential access的处理
	// 这里即使是oversize read也可能进入该条件？就是优先级上是顺序大于一切
	if ((offset == (ra->start + ra->size - ra->async_size) ||
	     offset == (ra->start + ra->size))) {
		ra->start += ra->size;
		// get_next_ra_size()用于增加ra->size窗口
		// 如果当前size大于等于max_pages/16，则按2倍增，否则按4倍增，但不超过max_pages
		ra->size = get_next_ra_size(ra, max_pages);
		ra->async_size = ra->size;
		goto readit;
	}

	/*
	 * Hit a marked page without valid readahead state.
	 * E.g. interleaved reads.
	 * Query the pagecache for async_size, which normally equals to
	 * readahead size. Ramp it up and use it as the new readahead size.
	 */
	// 交织读，假装命中readahead flag
	// 因为这个flag刚好跨越size - async size和async size，所以认为是interleaved
	if (hit_readahead_marker) {
		pgoff_t start;

		rcu_read_lock();
		// 从offset + 1到max_pages找出第一个没有cache的下标，从而更新请求
		start = page_cache_next_hole(mapping, offset + 1, max_pages);
		rcu_read_unlock();

		if (!start || start - offset > max_pages)
			return 0;

		// 太绕了。。放过我吧，能不能写点注释
		ra->start = start;
		ra->size = start - offset;	/* old async_size */
		ra->size += req_size;
		ra->size = get_next_ra_size(ra, max_pages);
		ra->async_size = ra->size;
		goto readit;
	}

	/*
	 * oversize read
	 */
	// 过大读，读请求会被截断，ra->size会设为max_pages
	if (req_size > max_pages)
		goto initial_readahead;

	/*
	 * sequential cache miss
	 * trivial case: (offset - prev_offset) == 1
	 * unaligned reads: (offset - prev_offset) == 0
	 */
	// 这个触发就很苛刻了，我认为能进入的情况只有页面缺失时触发同步预读
	// 不然哪来的ra->start与async边界只差1？完全重复的话会触发顺序读流程
	prev_offset = (unsigned long long)ra->prev_pos >> PAGE_SHIFT;
	if (offset - prev_offset <= 1UL)
		goto initial_readahead;

	/*
	 * Query the page cache and look for the traces(cached history pages)
	 * that a sequential stream would leave behind.
	 */
	// TODO history pages
	if (try_context_readahead(mapping, ra, offset, req_size, max_pages))
		goto readit;

	/*
	 * standalone, small random read
	 * Read as is, and do not pollute the readahead state.
	 */
	return __do_page_cache_readahead(mapping, filp, offset, req_size, 0);

initial_readahead:
	ra->start = offset;
	// get_init_ra_size()大概的做法
	// 1. 对req_size进行roundup，得到2^n的上取整，记为size
	// 2. 如果size <= max_pages/32，返回size*4
	// 3. 如果size <= max_pages/4，返回size*2
	// 4. 否则返回max_pages
	// <del>不管怎样，ra->size最终都大于等于req_size</del>
	// 会逼近max_pages但不超过该阈值
	// 这里与get_next_ra_size的增长策略共同点在于，(req_/ra)size越大，增长会越保守
	ra->size = get_init_ra_size(req_size, max_pages);
	// <del>async_size前一段差值还好理解，最后一个为啥是ra->size</del>
	// 推测是至少要分担一定比例的size，因为返回的可能是size*4 / size*2 / max，不希望async获得一个过小的值
	// async_size总是会尽可能尝试拿到ra->size同款大小
	ra->async_size = ra->size > req_size ? ra->size - req_size : ra->size;

// 提交readahead申请，附带一个合并请求的优化
readit:
	/*
	 * Will this read hit the readahead marker made by itself?
	 * If so, trigger the readahead marker hit now, and merge
	 * the resulted next readahead window into the current one.
	 * Take care of maximum IO pages as above.
	 */
	// 提前判断，加大力度
	// 这个应该对异步预读无效？（offset != ra->start）
	// 同步预读中的顺序读则基本会进入
	if (offset == ra->start && ra->size == ra->async_size) {
		add_pages = get_next_ra_size(ra, max_pages);
		if (ra->size + add_pages <= max_pages) {
			ra->async_size = add_pages;
			ra->size += add_pages;
		} else {
			ra->size = max_pages;
			ra->async_size = max_pages >> 1;
		}
	}

	// 这里真的要读了
	return ra_submit(ra, mapping, filp);
}

/*
 * Submit IO for the read-ahead request in file_ra_state.
 */
static inline unsigned long ra_submit(struct file_ra_state *ra,
		struct address_space *mapping, struct file *filp)
{
	return __do_page_cache_readahead(mapping, filp,
					ra->start, ra->size, ra->async_size);
}

/*
 * __do_page_cache_readahead() actually reads a chunk of disk.  It allocates
 * the pages first, then submits them for I/O. This avoids the very bad
 * behaviour which would occur if page allocations are causing VM writeback.
 * We really don't want to intermingle reads and writes like that.
 *
 * Returns the number of pages requested, or the maximum amount of I/O allowed.
 */
unsigned int __do_page_cache_readahead(struct address_space *mapping,
		struct file *filp, pgoff_t offset, unsigned long nr_to_read,
		unsigned long lookahead_size)
{
	struct inode *inode = mapping->host;
	struct page *page;
	unsigned long end_index;	/* The last page we want to read */
	// 大概意思是通过page_pool链表尽可能收集在一块再read
	LIST_HEAD(page_pool);
	// 下面流程大概扫了一眼，应该是要保证page_pool是连续的，
	// 如果中间发现某个page存在了，那就要断开，先把当前page_pool读完，
	// 然后清空nr_pages，再跳过当前这个page继续走下去

	int page_idx;
	unsigned int nr_pages = 0;
	loff_t isize = i_size_read(inode);
	gfp_t gfp_mask = readahead_gfp_mask(mapping);

	if (isize == 0)
		goto out;

	end_index = ((isize - 1) >> PAGE_SHIFT);

	/*
	 * Preallocate as many pages as we will need.
	 */
	for (page_idx = 0; page_idx < nr_to_read; page_idx++) {
		pgoff_t page_offset = offset + page_idx;

		if (page_offset > end_index)
			break;

		rcu_read_lock();
		page = radix_tree_lookup(&mapping->i_pages, page_offset);
		rcu_read_unlock();
		if (page && !radix_tree_exceptional_entry(page)) {
			/*
			 * Page already present?  Kick off the current batch of
			 * contiguous pages before continuing with the next
			 * batch.
			 */
			// Question. 这算是预读缓存命中问题？直观上会导致IO吞吐量的减少
			if (nr_pages)
				read_pages(mapping, filp, &page_pool, nr_pages,
						gfp_mask);
			nr_pages = 0;
			continue;
		}

		page = __page_cache_alloc(gfp_mask);
		if (!page)
			break;
		page->index = page_offset;
		list_add(&page->lru, &page_pool);
		if (page_idx == nr_to_read - lookahead_size)
			// 从前面的流程可以看到，并不会在已缓存的页面打上readahead flag（在上面已经continue跳过了）
			SetPageReadahead(page);
		nr_pages++;
	}

	/*
	 * Now start the IO.  We ignore I/O errors - if the page is not
	 * uptodate then the caller will launch readpage again, and
	 * will then handle the error.
	 */
	if (nr_pages)
		read_pages(mapping, filp, &page_pool, nr_pages, gfp_mask);
	BUG_ON(!list_empty(&page_pool));
out:
	return nr_pages;
}

static int read_pages(struct address_space *mapping, struct file *filp,
		struct list_head *pages, unsigned int nr_pages, gfp_t gfp)
{
	struct blk_plug plug;
	unsigned page_idx;
	int ret;

	blk_start_plug(&plug);

	if (mapping->a_ops->readpages) {
		ret = mapping->a_ops->readpages(filp, mapping, pages, nr_pages);
		/* Clean up the remaining pages */
		put_pages_list(pages);
		goto out;
	}

	// 这里是文件系统连多页读函数readpages都没有才会触发
	for (page_idx = 0; page_idx < nr_pages; page_idx++) {
		struct page *page = lru_to_page(pages);
		list_del(&page->lru);
		if (!add_to_page_cache_lru(page, mapping, page->index, gfp))
			mapping->a_ops->readpage(filp, page);
		put_page(page);
	}
	ret = 0;

out:
	blk_finish_plug(&plug);

	return ret;
}