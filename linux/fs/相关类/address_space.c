// 文件：/include/linux/fs.h
// 版本：4.18.20

struct address_space {
	struct inode		*host;		/* owner: inode, block_device */
	struct radix_tree_root	i_pages;	/* cached pages */
	atomic_t		i_mmap_writable;/* count VM_SHARED mappings */
	struct rb_root_cached	i_mmap;		/* tree of private and shared mappings */
	struct rw_semaphore	i_mmap_rwsem;	/* protect tree, count, list */
	/* Protected by the i_pages lock */
	unsigned long		nrpages;	/* number of total pages */
	/* number of shadow or DAX exceptional entries */
	unsigned long		nrexceptional;
	pgoff_t			writeback_index;/* writeback starts here */
	const struct address_space_operations *a_ops;	/* methods */
	unsigned long		flags;		/* error bits */
	spinlock_t		private_lock;	/* for use by the address_space */
	gfp_t			gfp_mask;	/* implicit gfp mask for allocations */
	struct list_head	private_list;	/* for use by the address_space */
	void			*private_data;	/* ditto */
	errseq_t		wb_err;
} __attribute__((aligned(sizeof(long)))) __randomize_layout;