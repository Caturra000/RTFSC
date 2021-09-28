// 文件：/include/linux/blkdev.h

/*
 * Try to put the fields that are referenced together in the same cacheline.
 *
 * If you modify this structure, make sure to update blk_rq_init() and
 * especially blk_mq_rq_ctx_init() to take care of the added fields.
 */
// 构造过程见blk_rq_init()
struct request {
	struct request_queue *q;
	struct blk_mq_ctx *mq_ctx;

	int cpu;
	unsigned int cmd_flags;		/* op and common flags */
	req_flags_t rq_flags;

	int internal_tag;

	/* the following two fields are internal, NEVER access directly */
	unsigned int __data_len;	/* total data len */
	int tag;
	sector_t __sector;		/* sector cursor */

	struct bio *bio;
	struct bio *biotail;

	struct list_head queuelist;

	/*
	 * The hash is used inside the scheduler, and killed once the
	 * request reaches the dispatch list. The ipi_list is only used
	 * to queue the request for softirq completion, which is long
	 * after the request has been unhashed (and even removed from
	 * the dispatch list).
	 */
	union {
		struct hlist_node hash;	/* merge hash */
		struct list_head ipi_list;
	};

	/*
	 * The rb_node is only used inside the io scheduler, requests
	 * are pruned when moved to the dispatch queue. So let the
	 * completion_data share space with the rb_node.
	 */
	union {
		struct rb_node rb_node;	/* sort/lookup */
		struct bio_vec special_vec;
		void *completion_data;
		int error_count; /* for legacy drivers, don't use */
	};

	/*
	 * Three pointers are available for the IO schedulers, if they need
	 * more they have to dynamically allocate it.  Flush requests are
	 * never put on the IO scheduler. So let the flush fields share
	 * space with the elevator data.
	 */
	union {
		struct {
			struct io_cq		*icq;
			void			*priv[2];
		} elv;

		struct {
			unsigned int		seq;
			struct list_head	list;
			rq_end_io_fn		*saved_end_io;
		} flush;
	};

	struct gendisk *rq_disk;
	struct hd_struct *part;
	/* Time that I/O was submitted to the kernel. */
	u64 start_time_ns;
	/* Time that I/O was submitted to the device. */
	u64 io_start_time_ns;

#ifdef CONFIG_BLK_WBT
	unsigned short wbt_flags;
#endif
#ifdef CONFIG_BLK_DEV_THROTTLING_LOW
	unsigned short throtl_size;
#endif

	/*
	 * Number of scatter-gather DMA addr+len pairs after
	 * physical address coalescing is performed.
	 */
	unsigned short nr_phys_segments;

#if defined(CONFIG_BLK_DEV_INTEGRITY)
	unsigned short nr_integrity_segments;
#endif

	unsigned short write_hint;
	unsigned short ioprio;

	void *special;		/* opaque pointer available for LLD use */

	unsigned int extra_len;	/* length of alignment and padding */

	enum mq_rq_state state;
	refcount_t ref;

	unsigned int timeout;

	/* access through blk_rq_set_deadline, blk_rq_deadline */
	unsigned long __deadline;

	struct list_head timeout_list;

	union {
		struct __call_single_data csd;
		u64 fifo_time;
	};

	/*
	 * completion callback.
	 */
	rq_end_io_fn *end_io;
	void *end_io_data;

	/* for bidi */
	struct request *next_rq;

#ifdef CONFIG_BLK_CGROUP
	struct request_list *rl;		/* rl this rq is alloced from */
#endif
};