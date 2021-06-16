// 使用base的原因是为了异常安全

// _Deque_base成员仅有_Deque_impl _M_impl
// _Deque_impl为_Deque_base的内部类，继承_Tp_alloc_type，用于EBO和简化实现，并没有其它有效的函数
// _Deque_impl的成员有
// 	_Map_pointer _M_map;
// 	size_t _M_map_size;
// 	iterator _M_start;
// 	iterator _M_finish;

// _Deque_base的关键函数
// 	void _M_initialize_map(size_t);   // ##flag #0
//      void _M_create_nodes(_Map_pointer __nstart, _Map_pointer __nfinish); // ##flag #1
//      void _M_destroy_nodes(_Map_pointer __nstart, _Map_pointer __nfinish) _GLIBCXX_NOEXCEPT;
//      enum { _S_initial_map_size = 8 };


  /**
   *  Deque base class.  This class provides the unified face for %deque's
   *  allocation.  This class's constructor and destructor allocate and
   *  deallocate (but do not initialize) storage.  This makes %exception
   *  safety easier.
   *
   *  Nothing in this class ever constructs or destroys an actual Tp element.
   *  (Deque handles that itself.)  Only/All memory management is performed
   *  here.
  */
  template<typename _Tp, typename _Alloc>
    class _Deque_base
    {
    protected:
      typedef typename __gnu_cxx::__alloc_traits<_Alloc>::template
	rebind<_Tp>::other _Tp_alloc_type;
      typedef __gnu_cxx::__alloc_traits<_Tp_alloc_type>	 _Alloc_traits;

#if __cplusplus < 201103L
      typedef _Tp*					_Ptr;
      typedef const _Tp*				_Ptr_const;
#else
      typedef typename _Alloc_traits::pointer		_Ptr;
      typedef typename _Alloc_traits::const_pointer	_Ptr_const;
#endif

      typedef typename _Alloc_traits::template rebind<_Ptr>::other
	_Map_alloc_type;
      typedef __gnu_cxx::__alloc_traits<_Map_alloc_type> _Map_alloc_traits;

    public:
      typedef _Alloc		  allocator_type;

      allocator_type
      get_allocator() const _GLIBCXX_NOEXCEPT
      { return allocator_type(_M_get_Tp_allocator()); }

      typedef _Deque_iterator<_Tp, _Tp&, _Ptr>	  iterator;
      typedef _Deque_iterator<_Tp, const _Tp&, _Ptr_const>   const_iterator;

      _Deque_base()
      : _M_impl()
      { _M_initialize_map(0); }

      _Deque_base(size_t __num_elements)
      : _M_impl()
      { _M_initialize_map(__num_elements); }

      _Deque_base(const allocator_type& __a, size_t __num_elements)
      : _M_impl(__a)
      { _M_initialize_map(__num_elements); }

      _Deque_base(const allocator_type& __a)
      : _M_impl(__a)
      { /* Caller must initialize map. */ }

#if __cplusplus >= 201103L
      _Deque_base(_Deque_base&& __x, false_type)
      : _M_impl(__x._M_move_impl())
      { }

      _Deque_base(_Deque_base&& __x, true_type)
      : _M_impl(std::move(__x._M_get_Tp_allocator()))
      {
	_M_initialize_map(0);
	if (__x._M_impl._M_map)
	  this->_M_impl._M_swap_data(__x._M_impl);
      }

      _Deque_base(_Deque_base&& __x)
      : _Deque_base(std::move(__x), typename _Alloc_traits::is_always_equal{})
      { }

      _Deque_base(_Deque_base&& __x, const allocator_type& __a, size_t __n)
      : _M_impl(__a)
      {
	if (__x.get_allocator() == __a)
	  {
	    if (__x._M_impl._M_map)
	      {
		_M_initialize_map(0);
		this->_M_impl._M_swap_data(__x._M_impl);
	      }
	  }
	else
	  {
	    _M_initialize_map(__n);
	  }
      }
#endif

      ~_Deque_base() _GLIBCXX_NOEXCEPT;

    protected:
      typedef typename iterator::_Map_pointer _Map_pointer;

      //This struct encapsulates the implementation of the std::deque
      //standard container and at the same time makes use of the EBO
      //for empty allocators.
      struct _Deque_impl
      : public _Tp_alloc_type
      {
	_Map_pointer _M_map;
	size_t _M_map_size;
	iterator _M_start;
	iterator _M_finish;

	_Deque_impl()
	: _Tp_alloc_type(), _M_map(), _M_map_size(0),
	  _M_start(), _M_finish()
	{ }

	_Deque_impl(const _Tp_alloc_type& __a) _GLIBCXX_NOEXCEPT
	: _Tp_alloc_type(__a), _M_map(), _M_map_size(0),
	  _M_start(), _M_finish()
	{ }

#if __cplusplus >= 201103L
	_Deque_impl(_Deque_impl&&) = default;

	_Deque_impl(_Tp_alloc_type&& __a) noexcept
	: _Tp_alloc_type(std::move(__a)), _M_map(), _M_map_size(0),
	  _M_start(), _M_finish()
	{ }
#endif

	void _M_swap_data(_Deque_impl& __x) _GLIBCXX_NOEXCEPT
	{
	  using std::swap;
	  swap(this->_M_start, __x._M_start);
	  swap(this->_M_finish, __x._M_finish);
	  swap(this->_M_map, __x._M_map);
	  swap(this->_M_map_size, __x._M_map_size);
	}
      };

      _Tp_alloc_type&
      _M_get_Tp_allocator() _GLIBCXX_NOEXCEPT
      { return *static_cast<_Tp_alloc_type*>(&this->_M_impl); }

      const _Tp_alloc_type&
      _M_get_Tp_allocator() const _GLIBCXX_NOEXCEPT
      { return *static_cast<const _Tp_alloc_type*>(&this->_M_impl); }

      _Map_alloc_type
      _M_get_map_allocator() const _GLIBCXX_NOEXCEPT
      { return _Map_alloc_type(_M_get_Tp_allocator()); }

      _Ptr
      _M_allocate_node()
      {
	typedef __gnu_cxx::__alloc_traits<_Tp_alloc_type> _Traits;
	return _Traits::allocate(_M_impl, __deque_buf_size(sizeof(_Tp)));
      }

      void
      _M_deallocate_node(_Ptr __p) _GLIBCXX_NOEXCEPT
      {
	typedef __gnu_cxx::__alloc_traits<_Tp_alloc_type> _Traits;
	_Traits::deallocate(_M_impl, __p, __deque_buf_size(sizeof(_Tp)));
      }

      _Map_pointer
      _M_allocate_map(size_t __n)
      {
	_Map_alloc_type __map_alloc = _M_get_map_allocator();
	return _Map_alloc_traits::allocate(__map_alloc, __n); // 可以认为allocate(n)
      }

      void
      _M_deallocate_map(_Map_pointer __p, size_t __n) _GLIBCXX_NOEXCEPT
      {
	_Map_alloc_type __map_alloc = _M_get_map_allocator();
	_Map_alloc_traits::deallocate(__map_alloc, __p, __n);
      }

    protected:
      void _M_initialize_map(size_t);
      void _M_create_nodes(_Map_pointer __nstart, _Map_pointer __nfinish);
      void _M_destroy_nodes(_Map_pointer __nstart,
			    _Map_pointer __nfinish) _GLIBCXX_NOEXCEPT;
      enum { _S_initial_map_size = 8 };

      _Deque_impl _M_impl;

#if __cplusplus >= 201103L
    private:
      _Deque_impl
      _M_move_impl()
      {
	if (!_M_impl._M_map)
	  return std::move(_M_impl);

	// Create a copy of the current allocator.
	_Tp_alloc_type __alloc{_M_get_Tp_allocator()};
	// Put that copy in a moved-from state.
	_Tp_alloc_type __sink __attribute((__unused__)) {std::move(__alloc)};
	// Create an empty map that allocates using the moved-from allocator.
	_Deque_base __empty{__alloc};
	__empty._M_initialize_map(0);
	// Now safe to modify current allocator and perform non-throwing swaps.
	_Deque_impl __ret{std::move(_M_get_Tp_allocator())};
	_M_impl._M_swap_data(__ret);
	_M_impl._M_swap_data(__empty._M_impl);
	return __ret;
      }
#endif
    };

  template<typename _Tp, typename _Alloc>
    _Deque_base<_Tp, _Alloc>::
    ~_Deque_base() _GLIBCXX_NOEXCEPT
    {
      if (this->_M_impl._M_map)
	{
	  _M_destroy_nodes(this->_M_impl._M_start._M_node,
			   this->_M_impl._M_finish._M_node + 1);
	  _M_deallocate_map(this->_M_impl._M_map, this->_M_impl._M_map_size);
	}
    }

  /**
   *  @brief Layout storage.
   *  @param  __num_elements  The count of T's for which to allocate space
   *                          at first.
   *  @return   Nothing.
   *
   *  The initial underlying memory layout is a bit complicated...
  */
  template<typename _Tp, typename _Alloc>
    void
    _Deque_base<_Tp, _Alloc>::
    _M_initialize_map(size_t __num_elements)       // ##flag #0
    {
      const size_t __num_nodes = (__num_elements/ __deque_buf_size(sizeof(_Tp))  // map可能的大小（node个数）
				  + 1);

      this->_M_impl._M_map_size = std::max((size_t) _S_initial_map_size,    // this->_M_impl._M_map_size为真正用于allocate的大小
					   size_t(__num_nodes + 2));
      this->_M_impl._M_map = _M_allocate_map(this->_M_impl._M_map_size);    // 可以认为allocate(n)

      // For "small" maps (needing less than _M_map_size nodes), allocation
      // starts in the middle elements and grows outwards.  So nstart may be
      // the beginning of _M_map, but for small maps it may be as far in as
      // _M_map+3.

      // Trick. 从中间开始处理，push_front往左边走，push_back往右边走
      // Question. __nfinish仍是__num_nodes？__nstart前的空间何时处理？ push_front满了会如何扩容？
      _Map_pointer __nstart = (this->_M_impl._M_map
			       + (this->_M_impl._M_map_size - __num_nodes) / 2);
      _Map_pointer __nfinish = __nstart + __num_nodes;

      __try
	{ _M_create_nodes(__nstart, __nfinish); } // 每个node分配1大小
      __catch(...)
	{
	  _M_deallocate_map(this->_M_impl._M_map, this->_M_impl._M_map_size);
	  this->_M_impl._M_map = _Map_pointer();
	  this->_M_impl._M_map_size = 0;
	  __throw_exception_again;
	}

      this->_M_impl._M_start._M_set_node(__nstart);
      this->_M_impl._M_finish._M_set_node(__nfinish - 1);
      this->_M_impl._M_start._M_cur = _M_impl._M_start._M_first;
      this->_M_impl._M_finish._M_cur = (this->_M_impl._M_finish._M_first
					+ __num_elements
					% __deque_buf_size(sizeof(_Tp)));
    }

  template<typename _Tp, typename _Alloc>
    void
    _Deque_base<_Tp, _Alloc>::
    _M_create_nodes(_Map_pointer __nstart, _Map_pointer __nfinish) // ##flag #1
    {
      _Map_pointer __cur;
      __try
	{
	  for (__cur = __nstart; __cur < __nfinish; ++__cur)
	    *__cur = this->_M_allocate_node();
	}
      __catch(...)
	{
	  _M_destroy_nodes(__nstart, __cur);
	  __throw_exception_again;
	}
    }

  template<typename _Tp, typename _Alloc>
    void
    _Deque_base<_Tp, _Alloc>::
    _M_destroy_nodes(_Map_pointer __nstart,
		     _Map_pointer __nfinish) _GLIBCXX_NOEXCEPT
    {
      for (_Map_pointer __n = __nstart; __n < __nfinish; ++__n)
	_M_deallocate_node(*__n);
    }
