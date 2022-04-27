// 文件：/include/linux/slub_def.h

/*
 * Slab cache management.
 */
// 快速缓存描述符
//
// SLUB算法的层级可以这么分：快速缓存-slab-对象
// 各层级间都是一对多的关系
// 其中slab为一组连续的页
//
// 每个快速缓存包括一个per-cpu本地slab和一个全局slab链表
// 全局slab补充说明：NUMA架构下为一个全局slab链表数组，每node各一个全局slab链表
struct kmem_cache {
	// per-cpu的slab
	struct kmem_cache_cpu __percpu *cpu_slab;
	/* Used for retriving partial slabs etc */
	slab_flags_t flags;
	unsigned long min_partial;
	unsigned int size;	/* The size of an object including meta data */
	unsigned int object_size;/* The size of an object without meta data */
	unsigned int offset;	/* Free pointer offset. */
#ifdef CONFIG_SLUB_CPU_PARTIAL
	// 用于控制cpu partial的阈值
	/* Number of per cpu partial objects to keep around */
	unsigned int cpu_partial;
#endif
	struct kmem_cache_order_objects oo;

	/* Allocation and freeing of slabs */
	struct kmem_cache_order_objects max;
	struct kmem_cache_order_objects min;
	gfp_t allocflags;	/* gfp flags to use on each alloc */
	int refcount;		/* Refcount for slab cache destroy */
	void (*ctor)(void *);
	unsigned int inuse;		/* Offset to metadata */
	unsigned int align;		/* Alignment */
	unsigned int red_left_pad;	/* Left redzone padding size */
	const char *name;	/* Name (only for display!) */
	struct list_head list;	/* List of slab caches */
#ifdef CONFIG_SYSFS
	struct kobject kobj;	/* For sysfs */
	struct work_struct kobj_remove_work;
#endif
#ifdef CONFIG_MEMCG
	struct memcg_cache_params memcg_params;
	/* for propagation, maximum size of a stored attr */
	unsigned int max_attr_size;
#ifdef CONFIG_SYSFS
	struct kset *memcg_kset;
#endif
#endif

#ifdef CONFIG_SLAB_FREELIST_HARDENED
	unsigned long random;
#endif

#ifdef CONFIG_NUMA
	/*
	 * Defragmentation by allocating from a remote node.
	 */
	unsigned int remote_node_defrag_ratio;
#endif

#ifdef CONFIG_SLAB_FREELIST_RANDOM
	unsigned int *random_seq;
#endif

#ifdef CONFIG_KASAN
	struct kasan_cache kasan_info;
#endif

	unsigned int useroffset;	/* Usercopy region offset */
	unsigned int usersize;		/* Usercopy region size */

	// 全局slab链表数组
	struct kmem_cache_node *node[MAX_NUMNODES];
};