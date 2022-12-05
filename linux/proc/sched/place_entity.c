// 文件：/kernel/sched/fair.c

// 进行vruntime调整
static void
place_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int initial)
{
	u64 vruntime = cfs_rq->min_vruntime;

	/*
	 * The 'current' period is already promised to the current tasks,
	 * however the extra weight of the new task will slow them down a
	 * little, place the new task so that it fits in the slot that
	 * stays open at the end.
	 */
	if (initial && sched_feat(START_DEBIT))
		vruntime += sched_vslice(cfs_rq, se);

	/* sleeps up to a single latency don't count. */
	if (!initial) {
		unsigned long thresh = sysctl_sched_latency;

		/*
		 * Halve their sleep time's effect, to allow
		 * for a gentler effect of sleepers:
		 */
		if (sched_feat(GENTLE_FAIR_SLEEPERS))
			thresh >>= 1;

		vruntime -= thresh;
	}

	/* ensure we never gain time by being placed backwards. */
	se->vruntime = max_vruntime(se->vruntime, vruntime);
}

/*
 * We calculate the vruntime slice of a to-be-inserted task.
 *
 * vs = s/w
 */
// vslice大概是算出一个将要插入task的惩罚时间
static u64 sched_vslice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	return calc_delta_fair(sched_slice(cfs_rq, se), se);
}

/*
 * We calculate the wall-time slice from the period by taking a part
 * proportional to the weight.
 *
 * s = p*P[w/rw]
 */
static u64 sched_slice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	// 传入当前在队列的task个数，se可能在队列也可能不在
	// 算出来的应该是个墙上时间，但仍需要后面calc_delta调教转为se视角的虚拟时间
	u64 slice = __sched_period(cfs_rq->nr_running + !se->on_rq);

	for_each_sched_entity(se) {
		struct load_weight *load;
		struct load_weight lw;

		cfs_rq = cfs_rq_of(se);
		load = &cfs_rq->load;

		// 不知道啥原因se不在rq上，因此需要算上se重新调整rq上的load
		// （这里我还不太明白，不在rq上却仍统计进去，是否合理？）
		if (unlikely(!se->on_rq)) {
			lw = cfs_rq->load;

			update_load_add(&lw, se->load.weight);
			load = &lw;
		}
		slice = __calc_delta(slice, se->load.weight, load);
	}
	return slice;
}

/*
 * The idea is to set a period in which each task runs once.
 *
 * When there are too many tasks (sched_nr_latency) we have to stretch
 * this period because otherwise the slices get too small.
 *
 * p = (nr <= nl) ? l : l*nr/nl
 */
// 调度周期的计算流程，保证每个task至少跑一遍
// 大概是max(nr_running * 0.75ms, 6ms)，nr大于8就取左边
// 6ms并不是一个传统意义上的时间片概念，只是一个CPU密集型task的调度抢占延迟
static u64 __sched_period(unsigned long nr_running)
{
	// sched_nr_latency初始化为8，
	// 注释有说是相除得到的数，6 / 0.75 = 8
	/*
	* This value is kept at sysctl_sched_latency/sysctl_sched_min_granularity
	*/
	if (unlikely(nr_running > sched_nr_latency))
		// sysctl_sched_min_granularity定义如下，不考虑CPU对数大概就是0.75ms的意思
		/*
		 * Minimal preemption granularity for CPU-bound tasks:
		 *
		 * (default: 0.75 msec * (1 + ilog(ncpus)), units: nanoseconds)
		 */
		return nr_running * sysctl_sched_min_granularity;
	else
		// 搬运一下sysctl_sched_latency的解释，默认不考虑CPU对数为6ms
		/*
		* Targeted preemption latency for CPU-bound tasks:
		*
		* NOTE: this latency value is not the same as the concept of
		* 'timeslice length' - timeslices in CFS are of variable length
		* and have no persistent notion like in traditional, time-slice
		* based scheduling concepts.
		*
		* (to see the precise effective timeslice length of your workload,
		*  run vmstat and monitor the context-switches (cs) field)
		*
		* (default: 6ms * (1 + ilog(ncpus)), units: nanoseconds)
		*/
		return sysctl_sched_latency;
}

static inline void update_load_add(struct load_weight *lw, unsigned long inc)
{
	lw->weight += inc;
	lw->inv_weight = 0;
}
