// 文件：/mm/page_alloc.c
static void __meminit __init_single_page(struct page *page, unsigned long pfn,
				unsigned long zone, int nid)
{
	mm_zero_struct_page(page);
	set_page_links(page, zone, nid, pfn);
	init_page_count(page);
	page_mapcount_reset(page);
	page_cpupid_reset_last(page);

	INIT_LIST_HEAD(&page->lru);
// 这个宏见struct page注释
// 只在非常少数的体系结构中有定义
#ifdef WANT_PAGE_VIRTUAL
	/* The shift won't overflow because ZONE_NORMAL is below 4G. */
	if (!is_highmem_idx(zone))
		set_page_address(page, __va(pfn << PAGE_SHIFT));
#endif
}

// 文件：/include/linux/mm.h

/*
 * On some architectures it is expensive to call memset() for small sizes.
 * Those architectures should provide their own implementation of "struct page"
 * zeroing by defining this macro in <asm/pgtable.h>.
 */
// 考虑到小范围的memset可能是低效率的行为
// kernel允许替换到不同体系下的最优实现
#ifndef mm_zero_struct_page
#define mm_zero_struct_page(pp)  ((void)memset((pp), 0, sizeof(struct page)))
#endif


static inline void set_page_links(struct page *page, enum zone_type zone,
	unsigned long node, unsigned long pfn)
{
	set_page_zone(page, zone);
	set_page_node(page, node);
#ifdef SECTION_IN_PAGE_FLAGS
	set_page_section(page, pfn_to_section_nr(pfn));
#endif
}

// 文件：/include/linux/page_ref.h
/*
 * Setup the page count before being freed into the page allocator for
 * the first time (boot or memory hotplug)
 */
static inline void init_page_count(struct page *page)
{
	set_page_count(page, 1);
}

// 文件：/include/linux/mm.h
/*
 * The atomic page->_mapcount, starts from -1: so that transitions
 * both from it and to it can be tracked, using atomic_inc_and_test
 * and atomic_add_negative(-1).
 */
static inline void page_mapcount_reset(struct page *page)
{
	atomic_set(&(page)->_mapcount, -1);
}


// 文件：/arch/x86/include/asm/page.h
// 可以看出pfn（到pa再）到va是一个线性的关系
#define __va(x)			((void *)((unsigned long)(x)+PAGE_OFFSET))

// 文件：/include/linux/mm.h
// 前提：#if defined(WANT_PAGE_VIRTUAL)
// 差不多是对这个地址进行缓存的意思（如果是HIGHMEM的话）
//
// 像是线性映射就不一定必须要这个
// 比如直接从struct page的地址都能一步到位（通过section计算）拿到virtual（不是page结构体本身的地址）
// 详见__page_to_pfn()（总之拿到pfn一切好说，再套个__va就差不多了，大概。。）
static inline void set_page_address(struct page *page, void *address)
{
	page->virtual = address;
}
