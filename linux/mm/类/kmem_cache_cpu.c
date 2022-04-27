// 文件：/include/linux/slub_def.h

// per-cpu级别
struct kmem_cache_cpu {
	// 这里写到，freelist是指向next available object
	// 这里的freelist是本地无锁的
	// 而下面的page->freelist是常规的空闲链表
	// - 在一个slab刚成为本地slab（新产生）时
	// - freelist为空
	// - page->freelist指向本地slab首个空闲对象
	void **freelist;	/* Pointer to next available object */
	unsigned long tid;	/* Globally unique transaction id */
	// slab分配的起始页
	struct page *page;	/* The slab from which we are allocating */
// CONFIG_SLUB_CPU_PARTIAL默认配置为yes
#ifdef CONFIG_SLUB_CPU_PARTIAL
	// CPU的本地部分满slab的链表指针
	// - 当freelist和page->freelist为空时意味着本地slab是全满slab状态
	// - 从本地partial链表中取出一个slab来更新本地slab
	// 比较特殊的是，本地partial链表是单向链表，而在kmem_cache_node中的partial slab是双向链表
	struct page *partial;	/* Partially allocated frozen slabs */
#endif
#ifdef CONFIG_SLUB_STATS
	unsigned stat[NR_SLUB_STAT_ITEMS];
#endif
};