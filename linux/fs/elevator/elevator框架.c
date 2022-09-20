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

	err = e->ops.sq.elevator_init_fn(q, e);
	if (err)
		elevator_put(e);
out_unlock:
	mutex_unlock(&q->sysfs_lock);
	return err;
}




// 文件：/block/blk-settings.c
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
	q->nr_requests = BLKDEV_MAX_RQ;

	q->make_request_fn = mfn;
	blk_queue_dma_alignment(q, 511);
	blk_queue_congestion_threshold(q);
	q->nr_batching = BLK_BATCH_REQ;

	blk_set_default_limits(&q->limits);
}

// 文件：/block/blk-core.c

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
	if (!blk_queue_nomerges(q)) {
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
