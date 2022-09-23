// elevator框架，单队列（single queue）实现

// 框架上关注以下流程：
// - 初始化
// - 合并bio请求
// - 插入请求到调度队列
// - 派发请求到派发队列



////////////////////////////// 初始化流程


// 文件：/block/elevator.c
/*
 * Use the default elevator specified by config boot param for non-mq devices,
 * or by config option.  Don't try to load modules as we could be running off
 * async and request_module() isn't allowed from async.
 */
int elevator_init(struct request_queue *q)
{
	struct elevator_type *e = NULL;
	int err = 0;

	/*
	 * q->sysfs_lock must be held to provide mutual exclusion between
	 * elevator_switch() and here.
	 */
	mutex_lock(&q->sysfs_lock);
	if (unlikely(q->elevator))
		goto out_unlock;

	// 根据字符串chosen_elevator来获取elevator_type实例
	// 实际上会从静态变量elv_list中遍历获取，过程没啥意思
	if (*chosen_elevator) {
		e = elevator_get(q, chosen_elevator, false);
		if (!e)
			printk(KERN_ERR "I/O scheduler %s not found\n",
							chosen_elevator);
	}

	if (!e)
		e = elevator_get(q, CONFIG_DEFAULT_IOSCHED, false);
	if (!e) {
		printk(KERN_ERR
			"Default I/O scheduler not found. Using noop.\n");
		e = elevator_get(q, "noop", false);
	}

	// 取决于elevator算法
	// 通常是初始化具体实现用到的私有数据
	err = e->ops.sq.elevator_init_fn(q, e);
	if (err)
		elevator_put(e);
out_unlock:
	mutex_unlock(&q->sysfs_lock);
	return err;
}





////////////////////////////// 合并bio



// 合并bio是提交IO时执行的
// 见generic_make_request及make_request_fn

// 文件：/block/blk-settings.c
// 而make_request_fn注册流程为
/**
 * blk_queue_make_request - define an alternate make_request function for a device
 * @q:  the request queue for the device to be affected
 * @mfn: the alternate make_request function
 *
 * Description:
 *    The normal way for &struct bios to be passed to a device
 *    driver is for them to be collected into requests on a request
 *    queue, and then to allow the device driver to select requests
 *    off that queue when it is ready.  This works well for many block
 *    devices. However some block devices (typically virtual devices
 *    such as md or lvm) do not benefit from the processing on the
 *    request queue, and are served best by having the requests passed
 *    directly to them.  This can be achieved by providing a function
 *    to blk_queue_make_request().
 *
 * Caveat:
 *    The driver that does this *must* be able to deal appropriately
 *    with buffers in "highmemory". This can be accomplished by either calling
 *    kmap_atomic() to get a temporary kernel mapping, or by calling
 *    blk_queue_bounce() to create a buffer in normal memory.
 **/
void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn)
{
	/*
	 * set defaults
	 */
	// blkdev中该值设为128（Max # of requests）
	q->nr_requests = BLKDEV_MAX_RQ;

	q->make_request_fn = mfn;
	blk_queue_dma_alignment(q, 511);
	blk_queue_congestion_threshold(q);
	// BLK_BATCH_REQ为32（Number of requests a "batching" process may submit）
	q->nr_batching = BLK_BATCH_REQ;

	// queue_limits调参
	// 这种需要参考硬件特性
	blk_set_default_limits(&q->limits);
}

// 文件：/block/blk-core.c

// 这里注册了blk_queue_bio
int blk_init_allocated_queue(struct request_queue *q)
{
	WARN_ON_ONCE(q->mq_ops);

	q->fq = blk_alloc_flush_queue(q, NUMA_NO_NODE, q->cmd_size);
	if (!q->fq)
		return -ENOMEM;

	if (q->init_rq_fn && q->init_rq_fn(q, q->fq->flush_rq, GFP_KERNEL))
		goto out_free_flush_queue;

	if (blk_init_rl(&q->root_rl, q, GFP_KERNEL))
		goto out_exit_flush_rq;

	INIT_WORK(&q->timeout_work, blk_timeout_work);
	q->queue_flags		|= QUEUE_FLAG_DEFAULT;

	/*
	 * This also sets hw/phys segments, boundary and size
	 */
	blk_queue_make_request(q, blk_queue_bio);

	q->sg_reserved_size = INT_MAX;

	if (elevator_init(q))
		goto out_exit_flush_rq;
	return 0;

out_exit_flush_rq:
	if (q->exit_rq_fn)
		q->exit_rq_fn(q, q->fq->flush_rq);
out_free_flush_queue:
	blk_free_flush_queue(q->fq);
	q->fq = NULL;
	return -ENOMEM;
}

// 在这里发生bio合并
// 会先尝试合并到plug list里面合适的request
// 然后尝试合并到调度队列中的request
// 最后才构造新的request并插入（如果有蓄流就插入plug list）
static blk_qc_t blk_queue_bio(struct request_queue *q, struct bio *bio)
{
	struct blk_plug *plug;
	int where = ELEVATOR_INSERT_SORT;
	struct request *req, *free;
	unsigned int request_count = 0;
	unsigned int wb_acct;

	/*
	 * low level driver can indicate that it wants pages above a
	 * certain limit bounced to low memory (ie for highmem, or even
	 * ISA dma in theory)
	 */
	blk_queue_bounce(q, &bio);

	// bio split操作
	// 如果不支持scatter IO，则需要拆分bio
	blk_queue_split(q, &bio);

	if (!bio_integrity_prep(bio))
		return BLK_QC_T_NONE;

	if (op_is_flush(bio->bi_opf)) {
		spin_lock_irq(q->queue_lock);
		where = ELEVATOR_INSERT_FLUSH;
		goto get_rq;
	}

	/*
	 * Check if we can merge with the plugged list before grabbing
	 * any locks.
	 */
	// 测试qflags QUEUE_FLAG_NOMERGES（由driver给出），如果条件允许就尝试插入plug list
	if (!blk_queue_nomerges(q)) {
		// request_count: number of traversed plugged requests
		if (blk_attempt_plug_merge(q, bio, &request_count, NULL))
			return BLK_QC_T_NONE;
	} else
		request_count = blk_plug_queued_count(q);

	spin_lock_irq(q->queue_lock);

	switch (elv_merge(q, &req, bio)) {
	case ELEVATOR_BACK_MERGE:
		if (!bio_attempt_back_merge(q, req, bio))
			break;
		elv_bio_merged(q, req, bio);
		free = attempt_back_merge(q, req);
		if (free)
			__blk_put_request(q, free);
		else
			elv_merged_request(q, req, ELEVATOR_BACK_MERGE);
		goto out_unlock;
	case ELEVATOR_FRONT_MERGE:
		if (!bio_attempt_front_merge(q, req, bio))
			break;
		elv_bio_merged(q, req, bio);
		free = attempt_front_merge(q, req);
		if (free)
			__blk_put_request(q, free);
		else
			elv_merged_request(q, req, ELEVATOR_FRONT_MERGE);
		goto out_unlock;
	default:
		break;
	}

get_rq:
	wb_acct = wbt_wait(q->rq_wb, bio, q->queue_lock);

	/*
	 * Grab a free request. This is might sleep but can not fail.
	 * Returns with the queue unlocked.
	 */
	blk_queue_enter_live(q);
	req = get_request(q, bio->bi_opf, bio, 0, GFP_NOIO);
	if (IS_ERR(req)) {
		blk_queue_exit(q);
		__wbt_done(q->rq_wb, wb_acct);
		if (PTR_ERR(req) == -ENOMEM)
			bio->bi_status = BLK_STS_RESOURCE;
		else
			bio->bi_status = BLK_STS_IOERR;
		bio_endio(bio);
		goto out_unlock;
	}

	wbt_track(req, wb_acct);

	/*
	 * After dropping the lock and possibly sleeping here, our request
	 * may now be mergeable after it had proven unmergeable (above).
	 * We don't worry about that case for efficiency. It won't happen
	 * often, and the elevators are able to handle it.
	 */
	blk_init_request_from_bio(req, bio);

	if (test_bit(QUEUE_FLAG_SAME_COMP, &q->queue_flags))
		req->cpu = raw_smp_processor_id();

	plug = current->plug;
	if (plug) {
		/*
		 * If this is the first request added after a plug, fire
		 * of a plug trace.
		 *
		 * @request_count may become stale because of schedule
		 * out, so check plug list again.
		 */
		if (!request_count || list_empty(&plug->list))
			trace_block_plug(q);
		else {
			struct request *last = list_entry_rq(plug->list.prev);
			// request个数或者bytes总数超过阈值时，执行同步unplug
			if (request_count >= BLK_MAX_REQUEST_COUNT ||
			    blk_rq_bytes(last) >= BLK_PLUG_FLUSH_SIZE) {
				blk_flush_plug_list(plug, false);
				trace_block_plug(q);
			}
		}
		list_add_tail(&req->queuelist, &plug->list);
		blk_account_io_start(req, true);
	} else {
		spin_lock_irq(q->queue_lock);
		add_acct_request(q, req, where);
		__blk_run_queue(q);
out_unlock:
		spin_unlock_irq(q->queue_lock);
	}

	return BLK_QC_T_NONE;
}


////////////////////////////// 插入请求

// 文件：/block/elevator.c
void __elv_add_request(struct request_queue *q, struct request *rq, int where)
{
	trace_block_rq_insert(q, rq);

	blk_pm_add_request(q, rq);

	rq->q = q;

	if (rq->rq_flags & RQF_SOFTBARRIER) {
		/* barriers are scheduling boundary, update end_sector */
		if (!blk_rq_is_passthrough(rq)) {
			q->end_sector = rq_end_sector(rq);
			q->boundary_rq = rq;
		}
	} else if (!(rq->rq_flags & RQF_ELVPRIV) &&
		    (where == ELEVATOR_INSERT_SORT ||
		     where == ELEVATOR_INSERT_SORT_MERGE))
		where = ELEVATOR_INSERT_BACK;

	switch (where) {
	case ELEVATOR_INSERT_REQUEUE:
	case ELEVATOR_INSERT_FRONT:
		rq->rq_flags |= RQF_SOFTBARRIER;
		list_add(&rq->queuelist, &q->queue_head);
		break;

	case ELEVATOR_INSERT_BACK:
		rq->rq_flags |= RQF_SOFTBARRIER;
		elv_drain_elevator(q);
		list_add_tail(&rq->queuelist, &q->queue_head);
		/*
		 * We kick the queue here for the following reasons.
		 * - The elevator might have returned NULL previously
		 *   to delay requests and returned them now.  As the
		 *   queue wasn't empty before this request, ll_rw_blk
		 *   won't run the queue on return, resulting in hang.
		 * - Usually, back inserted requests won't be merged
		 *   with anything.  There's no point in delaying queue
		 *   processing.
		 */
		__blk_run_queue(q);
		break;

	case ELEVATOR_INSERT_SORT_MERGE:
		/*
		 * If we succeed in merging this request with one in the
		 * queue already, we are done - rq has now been freed,
		 * so no need to do anything further.
		 */
		if (elv_attempt_insert_merge(q, rq))
			break;
		/* fall through */
	case ELEVATOR_INSERT_SORT:
		BUG_ON(blk_rq_is_passthrough(rq));
		rq->rq_flags |= RQF_SORTED;
		q->nr_sorted++;
		if (rq_mergeable(rq)) {
			elv_rqhash_add(q, rq);
			if (!q->last_merge)
				q->last_merge = rq;
		}

		/*
		 * Some ioscheds (cfq) run q->request_fn directly, so
		 * rq cannot be accessed after calling
		 * elevator_add_req_fn.
		 */
		q->elevator->type->ops.sq.elevator_add_req_fn(q, rq);
		break;

	case ELEVATOR_INSERT_FLUSH:
		rq->rq_flags |= RQF_SOFTBARRIER;
		blk_insert_flush(rq);
		break;
	default:
		printk(KERN_ERR "%s: bad insertion point %d\n",
		       __func__, where);
		BUG();
	}
}





////////////////////////////// 派发请求





// 文件：/block/blk-core.c

/**
 * blk_peek_request - peek at the top of a request queue
 * @q: request queue to peek at
 *
 * Description:
 *     Return the request at the top of @q.  The returned request
 *     should be started using blk_start_request() before LLD starts
 *     processing it.
 *
 * Return:
 *     Pointer to the request at the top of @q if available.  Null
 *     otherwise.
 */
struct request *blk_peek_request(struct request_queue *q)
{
	struct request *rq;
	int ret;

	lockdep_assert_held(q->queue_lock);
	WARN_ON_ONCE(q->mq_ops);

	while ((rq = elv_next_request(q)) != NULL) {
		if (!(rq->rq_flags & RQF_STARTED)) {
			/*
			 * This is the first time the device driver
			 * sees this request (possibly after
			 * requeueing).  Notify IO scheduler.
			 */
			if (rq->rq_flags & RQF_SORTED)
				elv_activate_rq(q, rq);

			/*
			 * just mark as started even if we don't start
			 * it, a request that has been delayed should
			 * not be passed by new incoming requests
			 */
			rq->rq_flags |= RQF_STARTED;
			trace_block_rq_issue(q, rq);
		}

		if (!q->boundary_rq || q->boundary_rq == rq) {
			q->end_sector = rq_end_sector(rq);
			q->boundary_rq = NULL;
		}

		if (rq->rq_flags & RQF_DONTPREP)
			break;

		if (q->dma_drain_size && blk_rq_bytes(rq)) {
			/*
			 * make sure space for the drain appears we
			 * know we can do this because max_hw_segments
			 * has been adjusted to be one fewer than the
			 * device can handle
			 */
			rq->nr_phys_segments++;
		}

		if (!q->prep_rq_fn)
			break;

		ret = q->prep_rq_fn(q, rq);
		if (ret == BLKPREP_OK) {
			break;
		} else if (ret == BLKPREP_DEFER) {
			/*
			 * the request may have been (partially) prepped.
			 * we need to keep this request in the front to
			 * avoid resource deadlock.  RQF_STARTED will
			 * prevent other fs requests from passing this one.
			 */
			if (q->dma_drain_size && blk_rq_bytes(rq) &&
			    !(rq->rq_flags & RQF_DONTPREP)) {
				/*
				 * remove the space for the drain we added
				 * so that we don't add it again
				 */
				--rq->nr_phys_segments;
			}

			rq = NULL;
			break;
		} else if (ret == BLKPREP_KILL || ret == BLKPREP_INVALID) {
			rq->rq_flags |= RQF_QUIET;
			/*
			 * Mark this request as started so we don't trigger
			 * any debug logic in the end I/O path.
			 */
			blk_start_request(rq);
			__blk_end_request_all(rq, ret == BLKPREP_INVALID ?
					BLK_STS_TARGET : BLK_STS_IOERR);
		} else {
			printk(KERN_ERR "%s: bad return=%d\n", __func__, ret);
			break;
		}
	}

	return rq;
}


static struct request *elv_next_request(struct request_queue *q)
{
	struct request *rq;
	struct blk_flush_queue *fq = blk_get_flush_queue(q, NULL);

	WARN_ON_ONCE(q->mq_ops);

	while (1) {
		list_for_each_entry(rq, &q->queue_head, queuelist) {
			if (blk_pm_allow_request(rq))
				return rq;

			if (rq->rq_flags & RQF_SOFTBARRIER)
				break;
		}

		/*
		 * Flush request is running and flush request isn't queueable
		 * in the drive, we can hold the queue till flush request is
		 * finished. Even we don't do this, driver can't dispatch next
		 * requests and will requeue them. And this can improve
		 * throughput too. For example, we have request flush1, write1,
		 * flush 2. flush1 is dispatched, then queue is hold, write1
		 * isn't inserted to queue. After flush1 is finished, flush2
		 * will be dispatched. Since disk cache is already clean,
		 * flush2 will be finished very soon, so looks like flush2 is
		 * folded to flush1.
		 * Since the queue is hold, a flag is set to indicate the queue
		 * should be restarted later. Please see flush_end_io() for
		 * details.
		 */
		if (fq->flush_pending_idx != fq->flush_running_idx &&
				!queue_flush_queueable(q)) {
			fq->flush_queue_delayed = 1;
			return NULL;
		}
		if (unlikely(blk_queue_bypass(q)) ||
		    !q->elevator->type->ops.sq.elevator_dispatch_fn(q, 0))
			return NULL;
	}
}
