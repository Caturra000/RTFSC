// 文件：/mm/backing-dev.c


static int __init default_bdi_init(void)
{
	int err;

	// 创建writeback线程
	bdi_wq = alloc_workqueue("writeback", WQ_MEM_RECLAIM | WQ_FREEZABLE |
					      WQ_UNBOUND | WQ_SYSFS, 0);
	if (!bdi_wq)
		return -ENOMEM;

	err = bdi_init(&noop_backing_dev_info);

	return err;
}


static int bdi_init(struct backing_dev_info *bdi)
{
	int ret;

	bdi->dev = NULL;

	kref_init(&bdi->refcnt);
	bdi->min_ratio = 0;
	bdi->max_ratio = 100;
	bdi->max_prop_frac = FPROP_FRAC_BASE;
	INIT_LIST_HEAD(&bdi->bdi_list);
	INIT_LIST_HEAD(&bdi->wb_list);
	init_waitqueue_head(&bdi->wb_waitq);

	ret = cgwb_bdi_init(bdi);

	return ret;
}

static int cgwb_bdi_init(struct backing_dev_info *bdi)
{
	int ret;

	INIT_RADIX_TREE(&bdi->cgwb_tree, GFP_ATOMIC);
	bdi->cgwb_congested_tree = RB_ROOT;
	mutex_init(&bdi->cgwb_release_mutex);

	ret = wb_init(&bdi->wb, bdi, 1, GFP_KERNEL);
	if (!ret) {
		bdi->wb.memcg_css = &root_mem_cgroup->css;
		bdi->wb.blkcg_css = blkcg_root_css;
	}
	return ret;
}

static int wb_init(struct bdi_writeback *wb, struct backing_dev_info *bdi,
		   int blkcg_id, gfp_t gfp)
{
	int i, err;

	memset(wb, 0, sizeof(*wb));

	if (wb != &bdi->wb)
		bdi_get(bdi);
	wb->bdi = bdi;
	wb->last_old_flush = jiffies;
	INIT_LIST_HEAD(&wb->b_dirty);
	INIT_LIST_HEAD(&wb->b_io);
	INIT_LIST_HEAD(&wb->b_more_io);
	INIT_LIST_HEAD(&wb->b_dirty_time);
	spin_lock_init(&wb->list_lock);

	wb->bw_time_stamp = jiffies;
	wb->balanced_dirty_ratelimit = INIT_BW;
	wb->dirty_ratelimit = INIT_BW;
	wb->write_bandwidth = INIT_BW;
	wb->avg_write_bandwidth = INIT_BW;

	spin_lock_init(&wb->work_lock);
	INIT_LIST_HEAD(&wb->work_list);
	// 初始化delayed work
	INIT_DELAYED_WORK(&wb->dwork, wb_workfn);
	wb->dirty_sleep = jiffies;

	wb->congested = wb_congested_get_create(bdi, blkcg_id, gfp);
	if (!wb->congested) {
		err = -ENOMEM;
		goto out_put_bdi;
	}

	err = fprop_local_init_percpu(&wb->completions, gfp);
	if (err)
		goto out_put_cong;

	for (i = 0; i < NR_WB_STAT_ITEMS; i++) {
		err = percpu_counter_init(&wb->stat[i], 0, gfp);
		if (err)
			goto out_destroy_stat;
	}

	return 0;

out_destroy_stat:
	while (i--)
		percpu_counter_destroy(&wb->stat[i]);
	fprop_local_destroy_percpu(&wb->completions);
out_put_cong:
	wb_congested_put(wb->congested);
out_put_bdi:
	if (wb != &bdi->wb)
		bdi_put(bdi);
	return err;
}
