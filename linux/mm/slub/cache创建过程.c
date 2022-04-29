// 文件：/mm/slab_common.c

struct kmem_cache *
kmem_cache_create(const char *name, unsigned int size, unsigned int align,
		slab_flags_t flags, void (*ctor)(void *))
{
	return kmem_cache_create_usercopy(name, size, align, flags, 0, 0,
					  ctor);
}

/*
 * kmem_cache_create_usercopy - Create a cache.
 * @name: A string which is used in /proc/slabinfo to identify this cache.
 * @size: The size of objects to be created in this cache.
 * @align: The required alignment for the objects.
 * @flags: SLAB flags
 * @useroffset: Usercopy region offset
 * @usersize: Usercopy region size
 * @ctor: A constructor for the objects.
 *
 * Returns a ptr to the cache on success, NULL on failure.
 * Cannot be called within a interrupt, but can be interrupted.
 * The @ctor is run when new pages are allocated by the cache.
 *
 * The flags are
 *
 * %SLAB_POISON - Poison the slab with a known test pattern (a5a5a5a5)
 * to catch references to uninitialised memory.
 *
 * %SLAB_RED_ZONE - Insert `Red' zones around the allocated memory to check
 * for buffer overruns.
 *
 * %SLAB_HWCACHE_ALIGN - Align the objects in this cache to a hardware
 * cacheline.  This can be beneficial if you're counting cycles as closely
 * as davem.
 */
struct kmem_cache *
kmem_cache_create_usercopy(const char *name,
		  unsigned int size, unsigned int align,
		  slab_flags_t flags,
		  unsigned int useroffset, unsigned int usersize,
		  void (*ctor)(void *))
{
	struct kmem_cache *s = NULL;
	const char *cache_name;
	int err;

	get_online_cpus();
	get_online_mems();
	// TODO memcg
	memcg_get_cache_ids();

	mutex_lock(&slab_mutex);

	err = kmem_cache_sanity_check(name, size);
	if (err) {
		goto out_unlock;
	}

	/* Refuse requests with allocator specific flags */
	if (flags & ~SLAB_FLAGS_PERMITTED) {
		err = -EINVAL;
		goto out_unlock;
	}

	/*
	 * Some allocators will constraint the set of valid flags to a subset
	 * of all flags. We expect them to define CACHE_CREATE_MASK in this
	 * case, and we'll just provide them with a sanitized version of the
	 * passed flags.
	 */
	flags &= CACHE_CREATE_MASK;

	/* Fail closed on bad usersize of useroffset values. */
	if (WARN_ON(!usersize && useroffset) ||
	    WARN_ON(size < usersize || size - usersize < useroffset))
		usersize = useroffset = 0;

	if (!usersize)
		// 尝试使用kmem_cache alias
		// 如果返回非空，则不必真的创建了
		s = __kmem_cache_alias(name, size, align, flags, ctor);
	if (s)
		goto out_unlock;

	cache_name = kstrdup_const(name, GFP_KERNEL);
	if (!cache_name) {
		err = -ENOMEM;
		goto out_unlock;
	}

	s = create_cache(cache_name, size,
			 calculate_alignment(flags, align, size),
			 flags, useroffset, usersize, ctor, NULL, NULL);
	if (IS_ERR(s)) {
		err = PTR_ERR(s);
		kfree_const(cache_name);
	}

out_unlock:
	mutex_unlock(&slab_mutex);

	memcg_put_cache_ids();
	put_online_mems();
	put_online_cpus();

	if (err) {
		if (flags & SLAB_PANIC)
			panic("kmem_cache_create: Failed to create slab '%s'. Error %d\n",
				name, err);
		else {
			pr_warn("kmem_cache_create(%s) failed with error %d\n",
				name, err);
			dump_stack();
		}
		return NULL;
	}
	return s;
}

// 文件：/mm/slub.c
struct kmem_cache *
__kmem_cache_alias(const char *name, unsigned int size, unsigned int align,
		   slab_flags_t flags, void (*ctor)(void *))
{
	struct kmem_cache *s, *c;

	s = find_mergeable(size, align, flags, name, ctor);
	// 如果找到
	// 则修改对应的元数据
	// 并增加引用计数即可
	if (s) {
		s->refcount++;

		/*
		 * Adjust the object sizes so that we clear
		 * the complete object on kzalloc.
		 */
		s->object_size = max(s->object_size, size);
		s->inuse = max(s->inuse, ALIGN(size, sizeof(void *)));

		for_each_memcg_cache(c, s) {
			c->object_size = s->object_size;
			c->inuse = max(c->inuse, ALIGN(size, sizeof(void *)));
		}

		if (sysfs_slab_alias(s, name)) {
			s->refcount--;
			s = NULL;
		}
	}

	return s;
}

// 文件：/mm/slab_common.c

struct kmem_cache *find_mergeable(unsigned int size, unsigned int align,
		slab_flags_t flags, const char *name, void (*ctor)(void *))
{
	struct kmem_cache *s;

	// 首先是看当前自身的情况

	// 如果全局控制不允许merge
	if (slab_nomerge)
		return NULL;

	// 如果有custom ctor
	if (ctor)
		return NULL;

	size = ALIGN(size, sizeof(void *));
	align = calculate_alignment(flags, align, size);
	size = ALIGN(size, align);
	flags = kmem_cache_flags(size, flags, name, NULL);

	// 如果此次创建局部控制不允许merge
	if (flags & SLAB_NEVER_MERGE)
		return NULL;

	// 然后是看遍历其它cache的情况

	// 遍历slab_root_caches
	// 非memcg情况下，slab_root_caches就是(struct list_head) slab_caches
	// 否则为(struct list_head) slab_root_caches，通过memcg_params.__root_caches_node定位
	// 有空看下：https://lkml.iu.edu/hypermail/linux/kernel/1701.2/01782.html
	list_for_each_entry_reverse(s, &slab_root_caches, root_caches_node) {
		// 可能的情况：
		// slab_nomerge || (s->flags & SLAB_NEVER_MERGE)
		// !is_root_cache(s)
		// s->ctor || s->usersize
		// bootstrap阶段
		if (slab_unmergeable(s))
			continue;

		// 当前申请对象大小太大了
		if (size > s->size)
			continue;

		// flags标记不合适
		if ((flags & SLAB_MERGE_SAME) != (s->flags & SLAB_MERGE_SAME))
			continue;
		/*
		 * Check if alignment is compatible.
		 * Courtesy of Adrian Drzewiecki
		 */
		// 对齐兼容
		if ((s->size & ~(align - 1)) != s->size)
			continue;

		// size差距限制在一定范围内
		if (s->size - size >= sizeof(void *))
			continue;

		if (IS_ENABLED(CONFIG_SLAB) && align &&
			(align > s->align || s->align % align))
			continue;

		return s;
	}
	return NULL;
}

static struct kmem_cache *create_cache(const char *name,
		unsigned int object_size, unsigned int align,
		slab_flags_t flags, unsigned int useroffset,
		unsigned int usersize, void (*ctor)(void *),
		struct mem_cgroup *memcg, struct kmem_cache *root_cache)
{
	struct kmem_cache *s;
	int err;

	if (WARN_ON(useroffset + usersize > object_size))
		useroffset = usersize = 0;

	err = -ENOMEM;
	// 从`kmem_cache`中分配得到kmem_cache对象
	// 有意思的一点是，cache也是一种object
	s = kmem_cache_zalloc(kmem_cache, GFP_KERNEL);
	if (!s)
		goto out;

	s->name = name;
	s->size = s->object_size = object_size;
	s->align = align;
	s->ctor = ctor;
	s->useroffset = useroffset;
	s->usersize = usersize;

	err = init_memcg_params(s, memcg, root_cache);
	if (err)
		goto out_free_cache;

	err = __kmem_cache_create(s, flags);
	if (err)
		goto out_free_cache;

	s->refcount = 1;
	// 把当前cache加入到slab_caches
	list_add(&s->list, &slab_caches);
	memcg_link_cache(s);
out:
	if (err)
		return ERR_PTR(err);
	return s;

out_free_cache:
	destroy_memcg_params(s);
	kmem_cache_free(kmem_cache, s);
	goto out;
}

// 文件：/include/linux/slab.h
/*
 * Shortcuts
 */
static inline void *kmem_cache_zalloc(struct kmem_cache *k, gfp_t flags)
{
	return kmem_cache_alloc(k, flags | __GFP_ZERO);
}


// 文件：/mm/slub.c

void *kmem_cache_alloc(struct kmem_cache *s, gfp_t gfpflags)
{
	// 见对象分配流程
	void *ret = slab_alloc(s, gfpflags, _RET_IP_);

	trace_kmem_cache_alloc(_RET_IP_, ret, s->object_size,
				s->size, gfpflags);

	return ret;
}

int __kmem_cache_create(struct kmem_cache *s, slab_flags_t flags)
{
	int err;

	// 见cache打开流程
	err = kmem_cache_open(s, flags);
	if (err)
		return err;

	/* Mutex is not taken during early boot */
	if (slab_state <= UP)
		return 0;

	memcg_propagate_slab_attrs(s);
	err = sysfs_slab_add(s);
	if (err)
		__kmem_cache_release(s);

	return err;
}
