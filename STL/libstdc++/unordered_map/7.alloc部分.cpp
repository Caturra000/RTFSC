  /**
   * This type deals with all allocation and keeps an allocator instance through
   * inheritance to benefit from EBO when possible.
   */
  template<typename _NodeAlloc>
    struct _Hashtable_alloc : private _Hashtable_ebo_helper<0, _NodeAlloc>
    {
    private:
      using __ebo_node_alloc = _Hashtable_ebo_helper<0, _NodeAlloc>;
    public:
      using __node_type = typename _NodeAlloc::value_type;
      using __node_alloc_type = _NodeAlloc;
      // Use __gnu_cxx to benefit from _S_always_equal and al.
      using __node_alloc_traits = __gnu_cxx::__alloc_traits<__node_alloc_type>;

      using __value_alloc_traits = typename __node_alloc_traits::template
	rebind_traits<typename __node_type::value_type>;

      using __node_base = __detail::_Hash_node_base;
      using __bucket_type = __node_base*;      
      using __bucket_alloc_type =
	__alloc_rebind<__node_alloc_type, __bucket_type>;
      using __bucket_alloc_traits = std::allocator_traits<__bucket_alloc_type>;

      _Hashtable_alloc() = default;
      _Hashtable_alloc(const _Hashtable_alloc&) = default;
      _Hashtable_alloc(_Hashtable_alloc&&) = default;

      template<typename _Alloc>
	_Hashtable_alloc(_Alloc&& __a)
	  : __ebo_node_alloc(std::forward<_Alloc>(__a))
	{ }

      __node_alloc_type&
      _M_node_allocator()
      { return __ebo_node_alloc::_S_get(*this); }

      const __node_alloc_type&
      _M_node_allocator() const
      { return __ebo_node_alloc::_S_cget(*this); }

      template<typename... _Args>
	__node_type*
	_M_allocate_node(_Args&&... __args);

      void
      _M_deallocate_node(__node_type* __n);

      void
      _M_deallocate_node_ptr(__node_type* __n);

      // Deallocate the linked list of nodes pointed to by __n
      void
      _M_deallocate_nodes(__node_type* __n);

      __bucket_type*
      _M_allocate_buckets(std::size_t __n);

      void
      _M_deallocate_buckets(__bucket_type*, std::size_t __n);
    };

  // Definitions of class template _Hashtable_alloc's out-of-line member
  // functions.
  template<typename _NodeAlloc>
    template<typename... _Args>
      typename _Hashtable_alloc<_NodeAlloc>::__node_type*
      _Hashtable_alloc<_NodeAlloc>::_M_allocate_node(_Args&&... __args)
      {
	auto __nptr = __node_alloc_traits::allocate(_M_node_allocator(), 1);
	__node_type* __n = std::__to_address(__nptr);
	__try
	  {
	    ::new ((void*)__n) __node_type;
	    __node_alloc_traits::construct(_M_node_allocator(),
					   __n->_M_valptr(),
					   std::forward<_Args>(__args)...);
	    return __n;
	  }
	__catch(...)
	  {
	    __node_alloc_traits::deallocate(_M_node_allocator(), __nptr, 1);
	    __throw_exception_again;
	  }
      }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_node(__node_type* __n)
    {
      __node_alloc_traits::destroy(_M_node_allocator(), __n->_M_valptr());
      _M_deallocate_node_ptr(__n);
    }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_node_ptr(__node_type* __n)
    {
      typedef typename __node_alloc_traits::pointer _Ptr;
      auto __ptr = std::pointer_traits<_Ptr>::pointer_to(*__n);
      __n->~__node_type();
      __node_alloc_traits::deallocate(_M_node_allocator(), __ptr, 1);
    }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_nodes(__node_type* __n)
    {
      while (__n)
	{
	  __node_type* __tmp = __n;
	  __n = __n->_M_next();
	  _M_deallocate_node(__tmp);
	}
    }

  template<typename _NodeAlloc>
    typename _Hashtable_alloc<_NodeAlloc>::__bucket_type*
    _Hashtable_alloc<_NodeAlloc>::_M_allocate_buckets(std::size_t __n)
    {
      __bucket_alloc_type __alloc(_M_node_allocator());

      auto __ptr = __bucket_alloc_traits::allocate(__alloc, __n);
      __bucket_type* __p = std::__to_address(__ptr);
      __builtin_memset(__p, 0, __n * sizeof(__bucket_type));
      return __p;
    }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_buckets(__bucket_type* __bkts,
							std::size_t __n)
    {
      typedef typename __bucket_alloc_traits::pointer _Ptr;
      auto __ptr = std::pointer_traits<_Ptr>::pointer_to(*__bkts);
      __bucket_alloc_type __alloc(_M_node_allocator());
      __bucket_alloc_traits::deallocate(__alloc, __ptr, __n);
    }