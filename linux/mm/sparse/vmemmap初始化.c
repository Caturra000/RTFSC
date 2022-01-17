// 文件：/mm/sparse.c

/*
 * Allocate the accumulated non-linear sections, allocate a mem_map
 * for each and record the physical to section mapping.
 */
void __init sparse_init(void)
{
	unsigned long pnum;
	struct page *map;
	unsigned long *usemap;
	unsigned long **usemap_map;
	int size;
#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER
	int size2;
	struct page **map_map;
#endif

	/* see include/linux/mmzone.h 'struct mem_section' definition */
	BUILD_BUG_ON(!is_power_of_2(sizeof(struct mem_section)));

	/* Setup pageblock_order for HUGETLB_PAGE_SIZE_VARIABLE */
	set_pageblock_order();

	/*
	 * map is using big page (aka 2M in x86 64 bit)
	 * usemap is less one page (aka 24 bytes)
	 * so alloc 2M (with 2M align) and 24 bytes in turn will
	 * make next 2M slip to one more 2M later.
	 * then in big system, the memory will have a lot of holes...
	 * here try to allocate 2M pages continuously.
	 *
	 * powerpc need to call sparse_init_one_section right after each
	 * sparse_early_mem_map_alloc, so allocate usemap_map at first.
	 */
	size = sizeof(unsigned long *) * NR_MEM_SECTIONS;
	usemap_map = memblock_virt_alloc(size, 0);
	if (!usemap_map)
		panic("can not allocate usemap_map\n");
	alloc_usemap_and_memmap(sparse_early_usemaps_alloc_node,
							(void *)usemap_map);

#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER
	size2 = sizeof(struct page *) * NR_MEM_SECTIONS;
	map_map = memblock_virt_alloc(size2, 0);
	if (!map_map)
		panic("can not allocate map_map\n");
	alloc_usemap_and_memmap(sparse_early_mem_maps_alloc_node,
							(void *)map_map);
#endif

	for_each_present_section_nr(0, pnum) {
		usemap = usemap_map[pnum];
		if (!usemap)
			continue;

#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER
		map = map_map[pnum];
#else
		map = sparse_early_mem_map_alloc(pnum);
#endif
		if (!map)
			continue;

		sparse_init_one_section(__nr_to_section(pnum), pnum, map,
								usemap);
	}

	vmemmap_populate_print_last();

#ifdef CONFIG_SPARSEMEM_ALLOC_MEM_MAP_TOGETHER
	memblock_free_early(__pa(map_map), size2);
#endif
	memblock_free_early(__pa(usemap_map), size);
}




static struct page __init *sparse_early_mem_map_alloc(unsigned long pnum)
{
	struct page *map;
	struct mem_section *ms = __nr_to_section(pnum);
	int nid = sparse_early_nid(ms);

	map = sparse_mem_map_populate(pnum, nid, NULL);
	if (map)
		return map;

	pr_err("%s: sparsemem memory map backing failed some memory will not be available\n",
	       __func__);
	ms->section_mem_map = 0;
	return NULL;
}


// 文件：/mm/sparse-vmemmap.c
struct page * __meminit sparse_mem_map_populate(unsigned long pnum, int nid,
		struct vmem_altmap *altmap)
{
	unsigned long start;
	unsigned long end;
	struct page *map;

	map = pfn_to_page(pnum * PAGES_PER_SECTION);
	start = (unsigned long)map;
	end = (unsigned long)(map + PAGES_PER_SECTION);

	if (vmemmap_populate(start, end, nid, altmap))
		return NULL;

	return map;
}





// 文件：/arch/x86/mm/init_64.c
int __meminit vmemmap_populate(unsigned long start, unsigned long end, int node,
		struct vmem_altmap *altmap)
{
	int err;

	if (boot_cpu_has(X86_FEATURE_PSE))
		err = vmemmap_populate_hugepages(start, end, node, altmap);
	else if (altmap) {
		pr_err_once("%s: no cpu support for altmap allocations\n",
				__func__);
		err = -ENOMEM;
	} else
		err = vmemmap_populate_basepages(start, end, node);
	if (!err)
		sync_global_pgds(start, end - 1);
	return err;
}


// 文件：/mm/sparse-vmemmap.c

int __meminit vmemmap_populate_basepages(unsigned long start,
					 unsigned long end, int node)
{
	unsigned long addr = start;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	for (; addr < end; addr += PAGE_SIZE) {
		pgd = vmemmap_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;
		p4d = vmemmap_p4d_populate(pgd, addr, node);
		if (!p4d)
			return -ENOMEM;
		pud = vmemmap_pud_populate(p4d, addr, node);
		if (!pud)
			return -ENOMEM;
		pmd = vmemmap_pmd_populate(pud, addr, node);
		if (!pmd)
			return -ENOMEM;
		pte = vmemmap_pte_populate(pmd, addr, node);
		if (!pte)
			return -ENOMEM;
		vmemmap_verify(pte, node, addr, addr + PAGE_SIZE);
	}

	return 0;
}



pgd_t * __meminit vmemmap_pgd_populate(unsigned long addr, int node)
{
	pgd_t *pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd)) {
		void *p = vmemmap_alloc_block_zero(PAGE_SIZE, node);
		if (!p)
			return NULL;
		pgd_populate(&init_mm, pgd, p);
	}
	return pgd;
}


static void * __meminit vmemmap_alloc_block_zero(unsigned long size, int node)
{
	void *p = vmemmap_alloc_block(size, node);

	if (!p)
		return NULL;
	memset(p, 0, size);

	return p;
}


void * __meminit vmemmap_alloc_block(unsigned long size, int node)
{
	/* If the main allocator is up use that, fallback to bootmem. */
	if (slab_is_available()) {
		gfp_t gfp_mask = GFP_KERNEL|__GFP_RETRY_MAYFAIL|__GFP_NOWARN;
		int order = get_order(size);
		static bool warned;
		struct page *page;

		page = alloc_pages_node(node, gfp_mask, order);
		if (page)
			return page_address(page);

		if (!warned) {
			warn_alloc(gfp_mask & ~__GFP_NOWARN, NULL,
				   "vmemmap alloc failure: order:%u", order);
			warned = true;
		}
		return NULL;
	} else
		return __earlyonly_bootmem_alloc(node, size, size,
				__pa(MAX_DMA_ADDRESS));
}