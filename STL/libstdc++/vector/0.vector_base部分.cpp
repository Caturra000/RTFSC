  /// See bits/stl_deque.h's _Deque_base for an explanation.
  // 这里把stl_deque.h的注释搬过来
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
  // 大致就是base class负责内存管理（只allocate但不initialize），是出于异常安全的考虑


  // _Vector_base的成员仅有_Vector_impl _M_impl
  // struct _Vector_impl: public _Tp_alloc_type, public _Vector_impl_data

  // 首先分析allocator萃取得出的_Tp_alloc_type                                             TODO 感觉了解vector前应该先看allocator部分比较好，整理一下萃取部分
  // _Tp_alloc_type为__gnu_cxx::__alloc_traits<_Alloc>::template rebind<_Tp>::other
  //     __gnu_cxx::__alloc_traits<_Alloc>::template rebind<_Tp>::other ===> typename _Base_type::template rebind_alloc<_Tp>
  //     _Base_type::template rebind_alloc<_Tp> ===> std::allocator_traits<_Alloc>::template rebind_alloc<_Tp>;
  //     这里有偏特化，对于_Alloc为std::allocator的情况，using rebind_alloc = allocator<_Up>;
  //         对于其他情况如下，不过多分析
  //         using rebind_alloc = __alloc_rebind<_Alloc, _Tp>;
  //         using __alloc_rebind = typename __allocator_traits_base::template __rebind<_Alloc, _Up>::type
  //         struct __allocator_traits_base { struct __rebind : __replace_first_arg<_Tp, _Up> { }; } // 这里有SFINAE不止一个定义，篇幅略
  //             关于__replace_first_arg给出的解释是// Given Template<T, ...> and U return Template<U, ...>, otherwise invalid.
  // _Alloc我就默认是std::allocator<_Tp>，因此rebind后还是同一个allocator<_Tp>               TODO 这个rebind机制到底是为了处理什么？是针对特定类型Up的特化而可以强制rebind到另一个自定义allocator？
  // 之所以选择继承是因为空基类                                                              Question. 我觉得甚至不继承也可以？毕竟能从模板直接拿到allocator？是为了获取到具体的对象而不是allocator类型吗？
  // 我应该先看文档比较好，这一堆template和类型的推导对于一般的流程来说没意义

  // _Vector_impl_data则是实际存储用到的区域
  // 都是pointer类型的_M_start _M_finish _M_end_of_storage
  // 注意到空的构造也不会进行new



  // 到了_Vector_base才有_M_create_storage调用allocate的流程



  template<typename _Tp, typename _Alloc>
    struct _Vector_base
    {
      typedef typename __gnu_cxx::__alloc_traits<_Alloc>::template
	rebind<_Tp>::other _Tp_alloc_type;
      typedef typename __gnu_cxx::__alloc_traits<_Tp_alloc_type>::pointer
       	pointer;






// _Vector_impl_data 部分
      struct _Vector_impl_data
      {
	pointer _M_start;
	pointer _M_finish;
	pointer _M_end_of_storage;

	_Vector_impl_data() _GLIBCXX_NOEXCEPT
	: _M_start(), _M_finish(), _M_end_of_storage()
	{ }

#if __cplusplus >= 201103L
	_Vector_impl_data(_Vector_impl_data&& __x) noexcept
	: _M_start(__x._M_start), _M_finish(__x._M_finish),
	  _M_end_of_storage(__x._M_end_of_storage)
	{ __x._M_start = __x._M_finish = __x._M_end_of_storage = pointer(); }
#endif

	void
	_M_copy_data(_Vector_impl_data const& __x) _GLIBCXX_NOEXCEPT
	{
	  _M_start = __x._M_start;
	  _M_finish = __x._M_finish;
	  _M_end_of_storage = __x._M_end_of_storage;
	}

	void
	_M_swap_data(_Vector_impl_data& __x) _GLIBCXX_NOEXCEPT
	{
	  // Do not use std::swap(_M_start, __x._M_start), etc as it loses
	  // information used by TBAA.
	  _Vector_impl_data __tmp;
	  __tmp._M_copy_data(*this);
	  _M_copy_data(__x);
	  __x._M_copy_data(__tmp);
	}
      };







// _Vector_impl 部分
      struct _Vector_impl
	: public _Tp_alloc_type, public _Vector_impl_data
      {
	_Vector_impl() _GLIBCXX_NOEXCEPT_IF(
	    is_nothrow_default_constructible<_Tp_alloc_type>::value)
	: _Tp_alloc_type()
	{ }

	_Vector_impl(_Tp_alloc_type const& __a) _GLIBCXX_NOEXCEPT
	: _Tp_alloc_type(__a)
	{ }

#if __cplusplus >= 201103L
	// Not defaulted, to enforce noexcept(true) even when
	// !is_nothrow_move_constructible<_Tp_alloc_type>.
	_Vector_impl(_Vector_impl&& __x) noexcept
	: _Tp_alloc_type(std::move(__x)), _Vector_impl_data(std::move(__x))
	{ }

	_Vector_impl(_Tp_alloc_type&& __a) noexcept
	: _Tp_alloc_type(std::move(__a))
	{ }

	_Vector_impl(_Tp_alloc_type&& __a, _Vector_impl&& __rv) noexcept
	: _Tp_alloc_type(std::move(__a)), _Vector_impl_data(std::move(__rv))
	{ }
#endif

      };

// _Vector_impl 部分结束




    public:
      typedef _Alloc allocator_type;

      _Tp_alloc_type&
      _M_get_Tp_allocator() _GLIBCXX_NOEXCEPT
      { return this->_M_impl; }

      const _Tp_alloc_type&
      _M_get_Tp_allocator() const _GLIBCXX_NOEXCEPT
      { return this->_M_impl; }

      allocator_type
      get_allocator() const _GLIBCXX_NOEXCEPT
      { return allocator_type(_M_get_Tp_allocator()); }

#if __cplusplus >= 201103L
      _Vector_base() = default;
#else
      _Vector_base() { }
#endif

      _Vector_base(const allocator_type& __a) _GLIBCXX_NOEXCEPT
      : _M_impl(__a) { }

      // Kept for ABI compatibility.
#if !_GLIBCXX_INLINE_VERSION
      _Vector_base(size_t __n)
      : _M_impl()
      { _M_create_storage(__n); }
#endif

      _Vector_base(size_t __n, const allocator_type& __a)
      : _M_impl(__a)
      { _M_create_storage(__n); }

#if __cplusplus >= 201103L
      _Vector_base(_Vector_base&&) = default;

      // Kept for ABI compatibility.
# if !_GLIBCXX_INLINE_VERSION
      _Vector_base(_Tp_alloc_type&& __a) noexcept
      : _M_impl(std::move(__a)) { }

      _Vector_base(_Vector_base&& __x, const allocator_type& __a)
      : _M_impl(__a)
      {
	if (__x.get_allocator() == __a)
	  this->_M_impl._M_swap_data(__x._M_impl);
	else
	  {
	    size_t __n = __x._M_impl._M_finish - __x._M_impl._M_start;
	    _M_create_storage(__n);
	  }
      }
# endif

      _Vector_base(const allocator_type& __a, _Vector_base&& __x)
      : _M_impl(_Tp_alloc_type(__a), std::move(__x._M_impl))
      { }
#endif

      ~_Vector_base() _GLIBCXX_NOEXCEPT
      {
	_M_deallocate(_M_impl._M_start,
		      _M_impl._M_end_of_storage - _M_impl._M_start);
      }

    public:
      _Vector_impl _M_impl;

      pointer
      _M_allocate(size_t __n)
      {
	typedef __gnu_cxx::__alloc_traits<_Tp_alloc_type> _Tr;
	return __n != 0 ? _Tr::allocate(_M_impl, __n) : pointer();
      }

      void
      _M_deallocate(pointer __p, size_t __n)
      {
	typedef __gnu_cxx::__alloc_traits<_Tp_alloc_type> _Tr;
	if (__p)
	  _Tr::deallocate(_M_impl, __p, __n);
      }

    protected:
      void
      _M_create_storage(size_t __n)
      {
	this->_M_impl._M_start = this->_M_allocate(__n);
	this->_M_impl._M_finish = this->_M_impl._M_start;
	this->_M_impl._M_end_of_storage = this->_M_impl._M_start + __n;
      }
    };