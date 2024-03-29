// 文件：/mm/page_alloc.c

// 闲置的（free）和可用的（available）不是同一回事
// 如果评判一个系统的内存可用值就是闲置值，那就忽略了很多为其他可用的部分
// 比如有大块的cache是可以进一步利用的
//
// 这里available的计算是一个估算值而不是准确值，并且算法的小修改也是较为频繁的
long si_mem_available(void)
{
	long available;
	unsigned long pagecache;
	unsigned long wmark_low = 0;
	unsigned long pages[NR_LRU_LISTS];
	struct zone *zone;
	int lru;

	for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
		pages[lru] = global_node_page_state(NR_LRU_BASE + lru);

	for_each_zone(zone)
		wmark_low += zone->watermark[WMARK_LOW];

	/*
	 * Estimate the amount of memory available for userspace allocations,
	 * without causing swapping.
	 */
	// global_zone_page_state(NR_FREE_PAGES)对应于MemFree
	// 预留页比较复杂，可以通过cat /proc/zoneinfo推算
	//    设每个zone下 zone_reserve = zoneinfo.hi + max_i(protection[i])
	//    那么totalreserve_pages = \sum zone_reserve
	// 初步得到的是free - reserved
	available = global_zone_page_state(NR_FREE_PAGES) - totalreserve_pages;

	/*
	 * Not all the page cache can be freed, otherwise the system will
	 * start swapping. Assume at least half of the page cache, or the
	 * low watermark worth of cache, needs to stay.
	 */
	// 这里pagecache变量是指认为是可以利用的cache
	// 是LRU中active file + inactive file之和
	pagecache = pages[LRU_ACTIVE_FILE] + pages[LRU_INACTIVE_FILE];
	// 除去（所有zone的）低水位（之和）的预留内存，认为都可利用（如果pagecache过小，则只利用一半）
	pagecache -= min(pagecache / 2, wmark_low);
	// 继续加上 可利用pagecache 部分
	available += pagecache;

	/*
	 * Part of the reclaimable slab consists of items that are in use,
	 * and cannot be freed. Cap this estimate at the low watermark.
	 */
	// 加上slab的部分reclaimable
	available += global_node_page_state(NR_SLAB_RECLAIMABLE) -
		     min(global_node_page_state(NR_SLAB_RECLAIMABLE) / 2,
			 wmark_low);

	/*
	 * Part of the kernel memory, which can be released under memory
	 * pressure.
	 */
	available += global_node_page_state(NR_INDIRECTLY_RECLAIMABLE_BYTES) >>
		PAGE_SHIFT;

	if (available < 0)
		available = 0;
	return available;
}
