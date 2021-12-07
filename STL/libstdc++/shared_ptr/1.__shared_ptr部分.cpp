  template<typename _Tp, _Lock_policy _Lp>
    class __shared_ptr
    : public __shared_ptr_access<_Tp, _Lp>
    {
    public:
      using element_type = typename remove_extent<_Tp>::type;

    private:
      // Constraint for taking ownership of a pointer of type _Yp*:
      template<typename _Yp>
	using _SafeConv
	  = typename enable_if<__sp_is_constructible<_Tp, _Yp>::value>::type;

      // Constraint for construction from shared_ptr and weak_ptr:
      template<typename _Yp, typename _Res = void>
	using _Compatible = typename
	  enable_if<__sp_compatible_with<_Yp*, _Tp*>::value, _Res>::type;

      // Constraint for assignment from shared_ptr and weak_ptr:
      template<typename _Yp>
	using _Assignable = _Compatible<_Yp, __shared_ptr&>;

      // Constraint for construction from unique_ptr:
      template<typename _Yp, typename _Del, typename _Res = void,
	       typename _Ptr = typename unique_ptr<_Yp, _Del>::pointer>
	using _UniqCompatible = typename enable_if<__and_<
	  __sp_compatible_with<_Yp*, _Tp*>, is_convertible<_Ptr, element_type*>
	  >::value, _Res>::type;

      // Constraint for assignment from unique_ptr:
      template<typename _Yp, typename _Del>
	using _UniqAssignable = _UniqCompatible<_Yp, _Del, __shared_ptr&>;

    public:

#if __cplusplus > 201402L
      using weak_type = __weak_ptr<_Tp, _Lp>;
#endif

      constexpr __shared_ptr() noexcept
      : _M_ptr(0), _M_refcount()
      { }

      template<typename _Yp, typename = _SafeConv<_Yp>>
	explicit
	__shared_ptr(_Yp* __p)
	: _M_ptr(__p), _M_refcount(__p, typename is_array<_Tp>::type())
	{
	  static_assert( !is_void<_Yp>::value, "incomplete type" );
	  static_assert( sizeof(_Yp) > 0, "incomplete type" );
	  _M_enable_shared_from_this_with(__p);
	}

      template<typename _Yp, typename _Deleter, typename = _SafeConv<_Yp>>
	__shared_ptr(_Yp* __p, _Deleter __d)
	: _M_ptr(__p), _M_refcount(__p, std::move(__d))
	{
	  static_assert(__is_invocable<_Deleter&, _Yp*&>::value,
	      "deleter expression d(p) is well-formed");
	  _M_enable_shared_from_this_with(__p);
	}

      template<typename _Yp, typename _Deleter, typename _Alloc,
	       typename = _SafeConv<_Yp>>
	__shared_ptr(_Yp* __p, _Deleter __d, _Alloc __a)
	: _M_ptr(__p), _M_refcount(__p, std::move(__d), std::move(__a))
	{
	  static_assert(__is_invocable<_Deleter&, _Yp*&>::value,
	      "deleter expression d(p) is well-formed");
	  _M_enable_shared_from_this_with(__p);
	}

      template<typename _Deleter>
	__shared_ptr(nullptr_t __p, _Deleter __d)
	: _M_ptr(0), _M_refcount(__p, std::move(__d))
	{ }

      template<typename _Deleter, typename _Alloc>
        __shared_ptr(nullptr_t __p, _Deleter __d, _Alloc __a)
	: _M_ptr(0), _M_refcount(__p, std::move(__d), std::move(__a))
	{ }

      template<typename _Yp>
	__shared_ptr(const __shared_ptr<_Yp, _Lp>& __r,
		     element_type* __p) noexcept
	: _M_ptr(__p), _M_refcount(__r._M_refcount) // never throws
	{ }

      __shared_ptr(const __shared_ptr&) noexcept = default;
      __shared_ptr& operator=(const __shared_ptr&) noexcept = default;
      ~__shared_ptr() = default;

      template<typename _Yp, typename = _Compatible<_Yp>>
	__shared_ptr(const __shared_ptr<_Yp, _Lp>& __r) noexcept
	: _M_ptr(__r._M_ptr), _M_refcount(__r._M_refcount)
	{ }

      __shared_ptr(__shared_ptr&& __r) noexcept
      : _M_ptr(__r._M_ptr), _M_refcount()
      {
	_M_refcount._M_swap(__r._M_refcount);
	__r._M_ptr = 0;
      }

      template<typename _Yp, typename = _Compatible<_Yp>>
	__shared_ptr(__shared_ptr<_Yp, _Lp>&& __r) noexcept
	: _M_ptr(__r._M_ptr), _M_refcount()
	{
	  _M_refcount._M_swap(__r._M_refcount);
	  __r._M_ptr = 0;
	}

      template<typename _Yp, typename = _Compatible<_Yp>>
	explicit __shared_ptr(const __weak_ptr<_Yp, _Lp>& __r)
	: _M_refcount(__r._M_refcount) // may throw
	{
	  // It is now safe to copy __r._M_ptr, as
	  // _M_refcount(__r._M_refcount) did not throw.
	  _M_ptr = __r._M_ptr;
	}

      // If an exception is thrown this constructor has no effect.
      template<typename _Yp, typename _Del,
	       typename = _UniqCompatible<_Yp, _Del>>
	__shared_ptr(unique_ptr<_Yp, _Del>&& __r)
	: _M_ptr(__r.get()), _M_refcount()
	{
	  auto __raw = __to_address(__r.get());
	  _M_refcount = __shared_count<_Lp>(std::move(__r));
	  _M_enable_shared_from_this_with(__raw);
	}

#if __cplusplus <= 201402L && _GLIBCXX_USE_DEPRECATED
    protected:
      // If an exception is thrown this constructor has no effect.
      template<typename _Tp1, typename _Del,
	       typename enable_if<__and_<
		 __not_<is_array<_Tp>>, is_array<_Tp1>,
	         is_convertible<typename unique_ptr<_Tp1, _Del>::pointer, _Tp*>
	       >::value, bool>::type = true>
	__shared_ptr(unique_ptr<_Tp1, _Del>&& __r, __sp_array_delete)
	: _M_ptr(__r.get()), _M_refcount()
	{
	  auto __raw = __to_address(__r.get());
	  _M_refcount = __shared_count<_Lp>(std::move(__r));
	  _M_enable_shared_from_this_with(__raw);
	}
    public:
#endif

#if _GLIBCXX_USE_DEPRECATED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      // Postcondition: use_count() == 1 and __r.get() == 0
      template<typename _Yp, typename = _Compatible<_Yp>>
	__shared_ptr(auto_ptr<_Yp>&& __r);
#pragma GCC diagnostic pop
#endif

      constexpr __shared_ptr(nullptr_t) noexcept : __shared_ptr() { }

      template<typename _Yp>
	_Assignable<_Yp>
	operator=(const __shared_ptr<_Yp, _Lp>& __r) noexcept
	{
	  _M_ptr = __r._M_ptr;
	  _M_refcount = __r._M_refcount; // __shared_count::op= doesn't throw
	  return *this;
	}

#if _GLIBCXX_USE_DEPRECATED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      template<typename _Yp>
	_Assignable<_Yp>
	operator=(auto_ptr<_Yp>&& __r)
	{
	  __shared_ptr(std::move(__r)).swap(*this);
	  return *this;
	}
#pragma GCC diagnostic pop
#endif

      __shared_ptr&
      operator=(__shared_ptr&& __r) noexcept
      {
	__shared_ptr(std::move(__r)).swap(*this);
	return *this;
      }

      template<class _Yp>
	_Assignable<_Yp>
	operator=(__shared_ptr<_Yp, _Lp>&& __r) noexcept
	{
	  __shared_ptr(std::move(__r)).swap(*this);
	  return *this;
	}

      template<typename _Yp, typename _Del>
	_UniqAssignable<_Yp, _Del>
	operator=(unique_ptr<_Yp, _Del>&& __r)
	{
	  __shared_ptr(std::move(__r)).swap(*this);
	  return *this;
	}

      void
      reset() noexcept
      { __shared_ptr().swap(*this); }

      template<typename _Yp>
	_SafeConv<_Yp>
	reset(_Yp* __p) // _Yp must be complete.
	{
	  // Catch self-reset errors.
	  __glibcxx_assert(__p == 0 || __p != _M_ptr);
	  __shared_ptr(__p).swap(*this);
	}

      template<typename _Yp, typename _Deleter>
	_SafeConv<_Yp>
	reset(_Yp* __p, _Deleter __d)
	{ __shared_ptr(__p, std::move(__d)).swap(*this); }

      template<typename _Yp, typename _Deleter, typename _Alloc>
	_SafeConv<_Yp>
	reset(_Yp* __p, _Deleter __d, _Alloc __a)
        { __shared_ptr(__p, std::move(__d), std::move(__a)).swap(*this); }

      element_type*
      get() const noexcept
      { return _M_ptr; }

      explicit operator bool() const // never throws
      { return _M_ptr == 0 ? false : true; }

      bool
      unique() const noexcept
      { return _M_refcount._M_unique(); }

      long
      use_count() const noexcept
      { return _M_refcount._M_get_use_count(); }

      void
      swap(__shared_ptr<_Tp, _Lp>& __other) noexcept
      {
	std::swap(_M_ptr, __other._M_ptr);
	_M_refcount._M_swap(__other._M_refcount);
      }

      template<typename _Tp1>
	bool
	owner_before(__shared_ptr<_Tp1, _Lp> const& __rhs) const noexcept
	{ return _M_refcount._M_less(__rhs._M_refcount); }

      template<typename _Tp1>
	bool
	owner_before(__weak_ptr<_Tp1, _Lp> const& __rhs) const noexcept
	{ return _M_refcount._M_less(__rhs._M_refcount); }

    protected:
      // This constructor is non-standard, it is used by allocate_shared.
      template<typename _Alloc, typename... _Args>
	__shared_ptr(_Sp_alloc_shared_tag<_Alloc> __tag, _Args&&... __args)
	: _M_ptr(), _M_refcount(_M_ptr, __tag, std::forward<_Args>(__args)...)
	{ _M_enable_shared_from_this_with(_M_ptr); }

      template<typename _Tp1, _Lock_policy _Lp1, typename _Alloc,
	       typename... _Args>
	friend __shared_ptr<_Tp1, _Lp1>
	__allocate_shared(const _Alloc& __a, _Args&&... __args);

      // This constructor is used by __weak_ptr::lock() and
      // shared_ptr::shared_ptr(const weak_ptr&, std::nothrow_t).
      __shared_ptr(const __weak_ptr<_Tp, _Lp>& __r, std::nothrow_t)
      : _M_refcount(__r._M_refcount, std::nothrow)
      {
	_M_ptr = _M_refcount._M_get_use_count() ? __r._M_ptr : nullptr;
      }

      friend class __weak_ptr<_Tp, _Lp>;

    private:

      template<typename _Yp>
	using __esft_base_t = decltype(__enable_shared_from_this_base(
	      std::declval<const __shared_count<_Lp>&>(),
	      std::declval<_Yp*>()));

      // Detect an accessible and unambiguous enable_shared_from_this base.
      template<typename _Yp, typename = void>
	struct __has_esft_base
	: false_type { };

      template<typename _Yp>
	struct __has_esft_base<_Yp, __void_t<__esft_base_t<_Yp>>>
	: __not_<is_array<_Tp>> { }; // No enable shared_from_this for arrays

      template<typename _Yp, typename _Yp2 = typename remove_cv<_Yp>::type>
	typename enable_if<__has_esft_base<_Yp2>::value>::type
	_M_enable_shared_from_this_with(_Yp* __p) noexcept
	{
	  if (auto __base = __enable_shared_from_this_base(_M_refcount, __p))
	    __base->_M_weak_assign(const_cast<_Yp2*>(__p), _M_refcount);
	}

      template<typename _Yp, typename _Yp2 = typename remove_cv<_Yp>::type>
	typename enable_if<!__has_esft_base<_Yp2>::value>::type
	_M_enable_shared_from_this_with(_Yp*) noexcept
	{ }

      void*
      _M_get_deleter(const std::type_info& __ti) const noexcept
      { return _M_refcount._M_get_deleter(__ti); }

      template<typename _Tp1, _Lock_policy _Lp1> friend class __shared_ptr;
      template<typename _Tp1, _Lock_policy _Lp1> friend class __weak_ptr;

      template<typename _Del, typename _Tp1, _Lock_policy _Lp1>
	friend _Del* get_deleter(const __shared_ptr<_Tp1, _Lp1>&) noexcept;

      template<typename _Del, typename _Tp1>
	friend _Del* get_deleter(const shared_ptr<_Tp1>&) noexcept;

      element_type*	   _M_ptr;         // Contained pointer.
      __shared_count<_Lp>  _M_refcount;    // Reference counter.
    };
