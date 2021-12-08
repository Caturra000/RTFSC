  /**
   *  @brief  Create an object that is owned by a shared_ptr.
   *  @param  __args  Arguments for the @a _Tp object's constructor.
   *  @return A shared_ptr that owns the newly created object.
   *  @throw  std::bad_alloc, or an exception thrown from the
   *          constructor of @a _Tp.
   */
  template<typename _Tp, typename... _Args>
    inline shared_ptr<_Tp>
    make_shared(_Args&&... __args)
    {
      typedef typename std::remove_cv<_Tp>::type _Tp_nc;
      return std::allocate_shared<_Tp>(std::allocator<_Tp_nc>(),
				       std::forward<_Args>(__args)...);
    }

  /**
   *  @brief  Create an object that is owned by a shared_ptr.
   *  @param  __a     An allocator.
   *  @param  __args  Arguments for the @a _Tp object's constructor.
   *  @return A shared_ptr that owns the newly created object.
   *  @throw  An exception thrown from @a _Alloc::allocate or from the
   *          constructor of @a _Tp.
   *
   *  A copy of @a __a will be used to allocate memory for the shared_ptr
   *  and the new object.
   */
  template<typename _Tp, typename _Alloc, typename... _Args>
    inline shared_ptr<_Tp>
    allocate_shared(const _Alloc& __a, _Args&&... __args)
    {
      return shared_ptr<_Tp>(_Sp_alloc_shared_tag<_Alloc>{__a},
			     std::forward<_Args>(__args)...);
    }








  template<typename _Tp, _Lock_policy _Lp = __default_lock_policy,
	   typename... _Args>
    inline __shared_ptr<_Tp, _Lp>
    __make_shared(_Args&&... __args)
    {
      typedef typename std::remove_const<_Tp>::type _Tp_nc;
      return std::__allocate_shared<_Tp, _Lp>(std::allocator<_Tp_nc>(),
					      std::forward<_Args>(__args)...);
    }

  template<typename _Tp, _Lock_policy _Lp = __default_lock_policy,
	   typename _Alloc, typename... _Args>
    inline __shared_ptr<_Tp, _Lp>
    __allocate_shared(const _Alloc& __a, _Args&&... __args)
    {
      return __shared_ptr<_Tp, _Lp>(_Sp_alloc_shared_tag<_Alloc>{__a},
				    std::forward<_Args>(__args)...);
    }










  template<typename _Tp, typename _Alloc, _Lock_policy _Lp>
    class _Sp_counted_ptr_inplace final : public _Sp_counted_base<_Lp>
    {
      class _Impl : _Sp_ebo_helper<0, _Alloc>
      {
	typedef _Sp_ebo_helper<0, _Alloc>	_A_base;

      public:
	explicit _Impl(_Alloc __a) noexcept : _A_base(__a) { }

	_Alloc& _M_alloc() noexcept { return _A_base::_S_get(*this); }

	__gnu_cxx::__aligned_buffer<_Tp> _M_storage;
      };

    public:
      using __allocator_type = __alloc_rebind<_Alloc, _Sp_counted_ptr_inplace>;

      // Alloc parameter is not a reference so doesn't alias anything in __args
      template<typename... _Args>
	_Sp_counted_ptr_inplace(_Alloc __a, _Args&&... __args)
	: _M_impl(__a)
	{
	  // _GLIBCXX_RESOLVE_LIB_DEFECTS
	  // 2070.  allocate_shared should use allocator_traits<A>::construct
	  allocator_traits<_Alloc>::construct(__a, _M_ptr(),
	      std::forward<_Args>(__args)...); // might throw
	}

      ~_Sp_counted_ptr_inplace() noexcept { }

      virtual void
      _M_dispose() noexcept
      {
	allocator_traits<_Alloc>::destroy(_M_impl._M_alloc(), _M_ptr());
      }

      // Override because the allocator needs to know the dynamic type
      virtual void
      _M_destroy() noexcept
      {
	__allocator_type __a(_M_impl._M_alloc());
	__allocated_ptr<__allocator_type> __guard_ptr{ __a, this };
	this->~_Sp_counted_ptr_inplace();
      }

    private:
      friend class __shared_count<_Lp>; // To be able to call _M_ptr().

      // No longer used, but code compiled against old libstdc++ headers
      // might still call it from __shared_ptr ctor to get the pointer out.
      virtual void*
      _M_get_deleter(const std::type_info& __ti) noexcept override
      {
	auto __ptr = const_cast<typename remove_cv<_Tp>::type*>(_M_ptr());
	// Check for the fake type_info first, so we don't try to access it
	// as a real type_info object. Otherwise, check if it's the real
	// type_info for this class. With RTTI enabled we can check directly,
	// or call a library function to do it.
	if (&__ti == &_Sp_make_shared_tag::_S_ti()
	    ||
#if __cpp_rtti
	    __ti == typeid(_Sp_make_shared_tag)
#else
	    _Sp_make_shared_tag::_S_eq(__ti)
#endif
	   )
	  return __ptr;
	return nullptr;
      }

      _Tp* _M_ptr() noexcept { return _M_impl._M_storage._M_ptr(); }

      _Impl _M_impl;
    };














      template<typename _Tp, typename _Alloc, typename... _Args>
	__shared_count(_Tp*& __p, _Sp_alloc_shared_tag<_Alloc> __a,
		       _Args&&... __args)
	{
	  typedef _Sp_counted_ptr_inplace<_Tp, _Alloc, _Lp> _Sp_cp_type;
	  typename _Sp_cp_type::__allocator_type __a2(__a._M_a);
	  auto __guard = std::__allocate_guarded(__a2);
	  _Sp_cp_type* __mem = __guard.get();
	  auto __pi = ::new (__mem)
	    _Sp_cp_type(__a._M_a, std::forward<_Args>(__args)...);
	  __guard = nullptr;
	  _M_pi = __pi;
	  __p = __pi->_M_ptr();
	}