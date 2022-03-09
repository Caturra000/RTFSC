// 文件：/mm/page_alloc.c

/* Lock and remove page from the per-cpu list */
static struct page *rmqueue_pcplist(struct zone *preferred_zone,
			struct zone *zone, unsigned int order,
			gfp_t gfp_flags, int migratetype)
{
	struct per_cpu_pages *pcp;
	struct list_head *list;
	struct page *page;
	unsigned long flags;

	local_irq_save(flags);
	pcp = &this_cpu_ptr(zone->pageset)->pcp;
	list = &pcp->lists[migratetype];
	page = __rmqueue_pcplist(zone,  migratetype, pcp, list);
	if (page) {
		__count_zid_vm_events(PGALLOC, page_zonenum(page), 1 << order);
		zone_statistics(preferred_zone, zone);
	}
	local_irq_restore(flags);
	return page;
}

/* Remove page from the per-cpu list, caller must protect the list */
static struct page *__rmqueue_pcplist(struct zone *zone, int migratetype,
			struct per_cpu_pages *pcp,
			struct list_head *list)
{
	struct page *page;

	do {
		if (list_empty(list)) {
			pcp->count += rmqueue_bulk(zone, 0,
					pcp->batch, list,
					migratetype);
			if (unlikely(list_empty(list)))
				return NULL;
		}

		page = list_first_entry(list, struct page, lru);
		list_del(&page->lru);
		pcp->count--;
	} while (check_new_pcp(page));

	return page;
}

/*
 * Obtain a specified number of elements from the buddy allocator, all under
 * a single hold of the lock, for efficiency.  Add them to the supplied list.
 * Returns the number of new pages which were placed at *list.
 */
static int rmqueue_bulk(struct zone *zone, unsigned int order,
			unsigned long count, struct list_head *list,
			int migratetype)
{
	int i, alloced = 0;

	spin_lock(&zone->lock);
	for (i = 0; i < count; ++i) {
		struct page *page = __rmqueue(zone, order, migratetype);
		if (unlikely(page == NULL))
			break;

		if (unlikely(check_pcp_refill(page)))
			continue;

		/*
		 * Split buddy pages returned by expand() are received here in
		 * physical page order. The page is added to the tail of
		 * caller's list. From the callers perspective, the linked list
		 * is ordered by page number under some conditions. This is
		 * useful for IO devices that can forward direction from the
		 * head, thus also in the physical page order. This is useful
		 * for IO devices that can merge IO requests if the physical
		 * pages are ordered properly.
		 */
		list_add_tail(&page->lru, list);
		alloced++;
		if (is_migrate_cma(get_pcppage_migratetype(page)))
			__mod_zone_page_state(zone, NR_FREE_CMA_PAGES,
					      -(1 << order));
	}

	/*
	 * i pages were removed from the buddy list even if some leak due
	 * to check_pcp_refill failing so adjust NR_FREE_PAGES based
	 * on i. Do not confuse with 'alloced' which is the number of
	 * pages added to the pcp list.
	 */
	__mod_zone_page_state(zone, NR_FREE_PAGES, -(i << order));
	spin_unlock(&zone->lock);
	return alloced;
}
