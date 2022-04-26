// 文件：/mm/bootmem.c

/**
 * free_all_bootmem - release free pages to the buddy allocator
 *
 * Returns the number of pages actually released.
 */
unsigned long __init free_all_bootmem(void)
{
	unsigned long total_pages = 0;
	bootmem_data_t *bdata;

	reset_all_zones_managed_pages();

	list_for_each_entry(bdata, &bdata_list, list)
		total_pages += free_all_bootmem_core(bdata);

	totalram_pages += total_pages;

	return total_pages;
}

static unsigned long __init free_all_bootmem_core(bootmem_data_t *bdata)
{
	struct page *page;
	unsigned long *map, start, end, pages, cur, count = 0;

	if (!bdata->node_bootmem_map)
		return 0;

	map = bdata->node_bootmem_map;
	start = bdata->node_min_pfn;
	end = bdata->node_low_pfn;

	bdebug("nid=%td start=%lx end=%lx\n",
		bdata - bootmem_node_data, start, end);

	while (start < end) {
		unsigned long idx, vec;
		unsigned shift;

		idx = start - bdata->node_min_pfn;
		shift = idx & (BITS_PER_LONG - 1);
		/*
		 * vec holds at most BITS_PER_LONG map bits,
		 * bit 0 corresponds to start.
		 */
		vec = ~map[idx / BITS_PER_LONG];

		if (shift) {
			vec >>= shift;
			if (end - start >= BITS_PER_LONG)
				vec |= ~map[idx / BITS_PER_LONG + 1] <<
					(BITS_PER_LONG - shift);
		}
		/*
		 * If we have a properly aligned and fully unreserved
		 * BITS_PER_LONG block of pages in front of us, free
		 * it in one go.
		 */
		if (IS_ALIGNED(start, BITS_PER_LONG) && vec == ~0UL) {
			int order = ilog2(BITS_PER_LONG);

			__free_pages_bootmem(pfn_to_page(start), start, order);
			count += BITS_PER_LONG;
			start += BITS_PER_LONG;
		} else {
			cur = start;

			start = ALIGN(start + 1, BITS_PER_LONG);
			while (vec && cur != start) {
				if (vec & 1) {
					page = pfn_to_page(cur);
					__free_pages_bootmem(page, cur, 0);
					count++;
				}
				vec >>= 1;
				++cur;
			}
		}
	}

	cur = bdata->node_min_pfn;
	page = virt_to_page(bdata->node_bootmem_map);
	pages = bdata->node_low_pfn - bdata->node_min_pfn;
	pages = bootmem_bootmap_pages(pages);
	count += pages;
	while (pages--)
		__free_pages_bootmem(page++, cur++, 0);
	bdata->node_bootmem_map = NULL;

	bdebug("nid=%td released=%lx\n", bdata - bootmem_node_data, count);

	return count;
}

// 文件：/mm/page_alloc.c

void __init __free_pages_bootmem(struct page *page, unsigned long pfn,
							unsigned int order)
{
	if (early_page_uninitialised(pfn))
		return;
	return __free_pages_boot_core(page, order);
}

static void __init __free_pages_boot_core(struct page *page, unsigned int order)
{
	unsigned int nr_pages = 1 << order;
	struct page *p = page;
	unsigned int loop;

	prefetchw(p);
	for (loop = 0; loop < (nr_pages - 1); loop++, p++) {
		prefetchw(p + 1);
		__ClearPageReserved(p);
		set_page_count(p, 0);
	}
	__ClearPageReserved(p);
	set_page_count(p, 0);

	page_zone(page)->managed_pages += nr_pages;
	set_page_refcounted(page);
	__free_pages(page, order);
}
