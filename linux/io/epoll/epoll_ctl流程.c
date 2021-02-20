
/*
 * The following function implements the controller interface for
 * the eventpoll file that enables the insertion/removal/change of
 * file descriptors inside the interest set.
 */
SYSCALL_DEFINE4(epoll_ctl, int, epfd, int, op, int, fd,
		struct epoll_event __user *, event)
{
	struct epoll_event epds;

	// del不传递epds
	if (ep_op_has_event(op) &&
	    copy_from_user(&epds, event, sizeof(struct epoll_event)))
		return -EFAULT;

	return do_epoll_ctl(epfd, op, fd, &epds, false);
}

/* Tells if the epoll_ctl(2) operation needs an event copy from userspace */
static inline int ep_op_has_event(int op)
{
	return op != EPOLL_CTL_DEL;
}


// 1. 从fd拿到file
// 2. 检测file支持poll
// 3. EPOLLWAKEUP/EPOLLEXCLUSIVE相关[略]
// 4. 真正处理ep
// 5. 使用ep->mtx保护红黑树
// 6. 通过tf查找epi
// 7. 处理
int do_epoll_ctl(int epfd, int op, int fd, struct epoll_event *epds,
		 bool nonblock)
{
	int error;
	int full_check = 0;
	struct fd f, tf;
	struct eventpoll *ep;
	struct epitem *epi;
	struct eventpoll *tep = NULL;

	error = -EBADF;
	f = fdget(epfd);
	if (!f.file)
		goto error_return;

	/* Get the "struct file *" for the target file */
	tf = fdget(fd);
	if (!tf.file)
		goto error_fput;

	/* The target file descriptor must support poll */
	error = -EPERM;
	if (!file_can_poll(tf.file))   // 要求file_operations支持poll，内部实现为return file->f_op->poll;
		goto error_tgt_fput;

	/* Check if EPOLLWAKEUP is allowed */
	if (ep_op_has_event(op))
		ep_take_care_of_epollwakeup(epds); // 去除EPOLLWAKEUP


        
    /*
    * Request the handling of system wakeup events so as to prevent system suspends
    * from happening while those events are being processed.
    *
    * Assuming neither EPOLLET nor EPOLLONESHOT is set, system suspends will not be
    * re-allowed until epoll_wait is called again after consuming the wakeup
    * event(s).
    *
    * Requires CAP_BLOCK_SUSPEND
    */
    //#define EPOLLWAKEUP (1 << 29)


	/*
	 * We have to check that the file structure underneath the file descriptor
	 * the user passed to us _is_ an eventpoll file. And also we do not permit
	 * adding an epoll file descriptor inside itself.
	 */
	error = -EINVAL;
	// ep的f不能等同于epi的tf
	if (f.file == tf.file || !is_file_epoll(f.file)) // 内部实现为 return file->f_op == &eventpoll_fops;
		goto error_tgt_fput;

	/*
	 * epoll adds to the wakeup queue at EPOLL_CTL_ADD time only,
	 * so EPOLLEXCLUSIVE is not allowed for a EPOLL_CTL_MOD operation.
	 * Also, we do not currently supported nested exclusive wakeups.
	 */
	// TODO 惊群处理？ 参考：https://lwn.net/Articles/667087/  可能单独开一个分析文件，暂不关注这些
	if (ep_op_has_event(op) && (epds->events & EPOLLEXCLUSIVE)) {
		if (op == EPOLL_CTL_MOD)
			goto error_tgt_fput;
		if (op == EPOLL_CTL_ADD && (is_file_epoll(tf.file) ||
				(epds->events & ~EPOLLEXCLUSIVE_OK_BITS)))
			goto error_tgt_fput;
	}

	/*
	 * At this point it is safe to assume that the "private_data" contains
	 * our own data structure.
	 */
	ep = f.file->private_data;

	/*
	 * When we insert an epoll file descriptor, inside another epoll file
	 * descriptor, there is the change of creating closed loops, which are
	 * better be handled here, than in more critical paths. While we are
	 * checking for loops we also determine the list of files reachable
	 * and hang them on the tfile_check_list, so we can check that we
	 * haven't created too many possible wakeup paths.
	 *
	 * We do not need to take the global 'epumutex' on EPOLL_CTL_ADD when
	 * the epoll file descriptor is attaching directly to a wakeup source,
	 * unless the epoll file descriptor is nested. The purpose of taking the
	 * 'epmutex' on add is to prevent complex toplogies such as loops and
	 * deep wakeup paths from forming in parallel through multiple
	 * EPOLL_CTL_ADD operations.
	 */
	error = epoll_mutex_lock(&ep->mtx, 0, nonblock);
	if (error)
		goto error_tgt_fput;
	// TODO 应该是一些check
// SKIP START
	if (op == EPOLL_CTL_ADD) {
		if (!list_empty(&f.file->f_ep_links) || // f是否在某个epoll里被嵌套监听，略
				ep->gen == loop_check_gen ||
						is_file_epoll(tf.file)) {
			mutex_unlock(&ep->mtx);
			error = epoll_mutex_lock(&epmutex, 0, nonblock);
			if (error)
				goto error_tgt_fput;
			loop_check_gen++;
			full_check = 1;
			if (is_file_epoll(tf.file)) {
				error = -ELOOP;
				if (ep_loop_check(ep, tf.file) != 0)
					goto error_tgt_fput;
			} else {
				get_file(tf.file);
				list_add(&tf.file->f_tfile_llink,
							&tfile_check_list);
			}
			error = epoll_mutex_lock(&ep->mtx, 0, nonblock);
			if (error)
				goto error_tgt_fput;
			if (is_file_epoll(tf.file)) {
				tep = tf.file->private_data;
				error = epoll_mutex_lock(&tep->mtx, 1, nonblock);
				if (error) {
					mutex_unlock(&ep->mtx);
					goto error_tgt_fput;
				}
			}
		}
	}
// SKIP END
	/*
	 * Try to lookup the file inside our RB tree, Since we grabbed "mtx"
	 * above, we can be sure to be able to use the item looked up by
	 * ep_find() till we release the mutex.
	 */
	// 已经处于线程安全的状态，在红黑树中查找epi
	epi = ep_find(ep, tf.file, fd);

	error = -EINVAL;
	// 核心操作，分别对应于ep_insert / ep_remove / ep_modify
	switch (op) {
	case EPOLL_CTL_ADD:
		if (!epi) {
			epds->events |= EPOLLERR | EPOLLHUP; // 默认会监听EPOLLERR和EPOLLHUP
			error = ep_insert(ep, epds, tf.file, fd, full_check);
		} else // 不支持重复ADD
			error = -EEXIST;
		break;
	case EPOLL_CTL_DEL:
		if (epi)
			error = ep_remove(ep, epi);
		else
			error = -ENOENT;
		break;
	case EPOLL_CTL_MOD:
		if (epi) {
			if (!(epi->event.events & EPOLLEXCLUSIVE)) {
				epds->events |= EPOLLERR | EPOLLHUP;
				error = ep_modify(ep, epi, epds);
			}
		} else
			error = -ENOENT;
		break;
	}
	if (tep != NULL)
		mutex_unlock(&tep->mtx);
	mutex_unlock(&ep->mtx);

error_tgt_fput:
	if (full_check) {
		clear_tfile_check_list();
		loop_check_gen++;
		mutex_unlock(&epmutex);
	}

	fdput(tf);
error_fput:
	fdput(f);
error_return:

	return error;
}









/*
 * Must be called with "mtx" held.
 */
static int ep_insert(struct eventpoll *ep, const struct epoll_event *event,
		     struct file *tfile, int fd, int full_check)
{
	int error, pwake = 0;
	__poll_t revents;
	long user_watches;
	struct epitem *epi;
	struct ep_pqueue epq;

	lockdep_assert_irqs_enabled();

	user_watches = atomic_long_read(&ep->user->epoll_watches);
	if (unlikely(user_watches >= max_user_watches))
		return -ENOSPC;
	if (!(epi = kmem_cache_alloc(epi_cache, GFP_KERNEL)))
		return -ENOMEM;

	// 构造epi
	/* Item initialization follow here ... */
	INIT_LIST_HEAD(&epi->rdllink);
	INIT_LIST_HEAD(&epi->fllink);
	INIT_LIST_HEAD(&epi->pwqlist);
	epi->ep = ep;
	ep_set_ffd(&epi->ffd, tfile, fd); // 用tfile和fd构造ffd
	epi->event = *event;
	epi->nwait = 0;
	epi->next = EP_UNACTIVE_PTR;
	if (epi->event.events & EPOLLWAKEUP) {
		error = ep_create_wakeup_source(epi);
		if (error)
			goto error_create_wakeup_source;
	} else {
		RCU_INIT_POINTER(epi->ws, NULL);
	}

	/* Add the current item to the list of active epoll hook for this file */
	spin_lock(&tfile->f_lock);
	list_add_tail_rcu(&epi->fllink, &tfile->f_ep_links);
	spin_unlock(&tfile->f_lock);

	/*
	 * Add the current item to the RB tree. All RB tree operations are
	 * protected by "mtx", and ep_insert() is called with "mtx" held.
	 */
	ep_rbtree_insert(ep, epi);

	/* now check if we've created too many backpaths */
	error = -EINVAL;
	if (full_check && reverse_path_check())
		goto error_remove_epi;

	/* Initialize the poll table using the queue callback */
	// callback放入epq
	epq.epi = epi;
	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc);   // pt->_qproc = qproc; pt->_key = ~(__poll_t)0; /* all events enabled */

	/*
	 * Attach the item to the poll hooks and get current event bits.
	 * We can safely use the file* here because its usage count has
	 * been increased by the caller of this function. Note that after
	 * this operation completes, the poll callback can start hitting
	 * the new item.
	 */
	revents = ep_item_poll(epi, &epq.pt, 1);

	/*
	 * We have to check if something went wrong during the poll wait queue
	 * install process. Namely an allocation for a wait queue failed due
	 * high memory pressure.
	 */
	error = -ENOMEM;
	if (epi->nwait < 0)
		goto error_unregister;

	/* We have to drop the new item inside our item list to keep track of it */
	write_lock_irq(&ep->lock);

	/* record NAPI ID of new item if present */
	ep_set_busy_poll_napi_id(epi);

	/* If the file is already "ready" we drop it inside the ready list */
	if (revents && !ep_is_linked(epi)) {
		list_add_tail(&epi->rdllink, &ep->rdllist);
		ep_pm_stay_awake(epi);

		/* Notify waiting tasks that events are available */
		if (waitqueue_active(&ep->wq))
			wake_up(&ep->wq);
		if (waitqueue_active(&ep->poll_wait))
			pwake++;
	}

	write_unlock_irq(&ep->lock);

	atomic_long_inc(&ep->user->epoll_watches);

	/* We have to call this outside the lock */
	if (pwake)
		ep_poll_safewake(ep, NULL);

	return 0;

error_unregister:
	ep_unregister_pollwait(ep, epi);
error_remove_epi:
	spin_lock(&tfile->f_lock);
	list_del_rcu(&epi->fllink);
	spin_unlock(&tfile->f_lock);

	rb_erase_cached(&epi->rbn, &ep->rbr);

	/*
	 * We need to do this because an event could have been arrived on some
	 * allocated wait queue. Note that we don't care about the ep->ovflist
	 * list, since that is used/cleaned only inside a section bound by "mtx".
	 * And ep_insert() is called with "mtx" held.
	 */
	write_lock_irq(&ep->lock);
	if (ep_is_linked(epi))
		list_del_init(&epi->rdllink);
	write_unlock_irq(&ep->lock);

	wakeup_source_unregister(ep_wakeup_source(epi));

error_create_wakeup_source:
	kmem_cache_free(epi_cache, epi);

	return error;
}



/*
 * This is the callback that is used to add our wait queue to the
 * target file wakeup lists.
 */
// 放入epq的callback，主要处理pwq
static void ep_ptable_queue_proc(struct file *file, wait_queue_head_t *whead,
				 poll_table *pt)
{
	struct ep_pqueue *epq = container_of(pt, struct ep_pqueue, pt);
	struct epitem *epi = epq->epi;
	struct eppoll_entry *pwq;

	if (unlikely(!epi))	// an earlier allocation has failed
		return;

	pwq = kmem_cache_alloc(pwq_cache, GFP_KERNEL);
	if (unlikely(!pwq)) {
		epq->epi = NULL;
		return;
	}

	init_waitqueue_func_entry(&pwq->wait, ep_poll_callback);
	pwq->whead = whead;
	pwq->base = epi;
	if (epi->event.events & EPOLLEXCLUSIVE)
		add_wait_queue_exclusive(whead, &pwq->wait);
	else
		add_wait_queue(whead, &pwq->wait);
	pwq->next = epi->pwqlist;
	epi->pwqlist = pwq;
}





/*
 * Differs from ep_eventpoll_poll() in that internal callers already have
 * the ep->mtx so we need to start from depth=1, such that mutex_lock_nested()
 * is correctly annotated.
 */
static __poll_t ep_item_poll(const struct epitem *epi, poll_table *pt,
				 int depth)
{
	struct file *file = epi->ffd.file;
	__poll_t res;

	pt->_key = epi->event.events;
	if (!is_file_epoll(file))
		res = vfs_poll(file, pt);
	else
		res = __ep_eventpoll_poll(file, pt, depth);
	return res & epi->event.events;
}


static __poll_t __ep_eventpoll_poll(struct file *file, poll_table *wait, int depth)
{
	struct eventpoll *ep = file->private_data;
	LIST_HEAD(txlist);
	struct epitem *epi, *tmp;
	poll_table pt;
	__poll_t res = 0;

	init_poll_funcptr(&pt, NULL);

	/* Insert inside our poll wait queue */
	poll_wait(file, &ep->poll_wait, wait);

	/*
	 * Proceed to find out if wanted events are really available inside
	 * the ready list.
	 */
	mutex_lock_nested(&ep->mtx, depth);
	ep_start_scan(ep, &txlist);
	list_for_each_entry_safe(epi, tmp, &txlist, rdllink) {
		if (ep_item_poll(epi, &pt, depth + 1)) {
			res = EPOLLIN | EPOLLRDNORM;
			break;
		} else {
			/*
			 * Item has been dropped into the ready list by the poll
			 * callback, but it's not actually ready, as far as
			 * caller requested events goes. We can remove it here.
			 */
			__pm_relax(ep_wakeup_source(epi));
			list_del_init(&epi->rdllink);
		}
	}
	ep_done_scan(ep, &txlist);
	mutex_unlock(&ep->mtx);
	return res;
}

// 分布：/include/linux/poll.h
static inline void poll_wait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	if (p && p->_qproc && wait_address)
		p->_qproc(filp, wait_address, p);
}
