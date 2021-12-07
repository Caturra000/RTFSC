  template<typename _Tp, _Lock_policy _Lp>
    class __weak_ptr
    {
      template<typename _Yp, typename _Res = void>
	using _Compatible = typename
	  enable_if<__sp_compatible_with<_Yp*, _Tp*>::value, _Res>::type;

      // Constraint for assignment from shared_ptr and weak_ptr:
      template<typename _Yp>
	using _Assignable = _Compatible<_Yp, __weak_ptr&>;

    public:
      using element_type = typename remove_extent<_Tp>::type;

      constexpr __weak_ptr() noexcept
      : _M_ptr(nullptr), _M_refcount()
      { }

      __weak_ptr(const __weak_ptr&) noexcept = default;

      ~__weak_ptr() = default;

      // The "obvious" converting constructor implementation:
      //
      //  template<typename _Tp1>
      //    __weak_ptr(const __weak_ptr<_Tp1, _Lp>& __r)
      //    : _M_ptr(__r._M_ptr), _M_refcount(__r._M_refcount) // never throws
      //    { }
      //
      // has a serious problem.
      //
      //  __r._M_ptr may already have been invalidated. The _M_ptr(__r._M_ptr)
      //  conversion may require access to *__r._M_ptr (virtual inheritance).
      //
      // It is not possible to avoid spurious access violations since
      // in multithreaded programs __r._M_ptr may be invalidated at any point.
      template<typename _Yp, typename = _Compatible<_Yp>>
	__weak_ptr(const __weak_ptr<_Yp, _Lp>& __r) noexcept
	: _M_refcount(__r._M_refcount)
        { _M_ptr = __r.lock().get(); }

      template<typename _Yp, typename = _Compatible<_Yp>>
	__weak_ptr(const __shared_ptr<_Yp, _Lp>& __r) noexcept
	: _M_ptr(__r._M_ptr), _M_refcount(__r._M_refcount)
	{ }

      __weak_ptr(__weak_ptr&& __r) noexcept
      : _M_ptr(__r._M_ptr), _M_refcount(std::move(__r._M_refcount))
      { __r._M_ptr = nullptr; }

      template<typename _Yp, typename = _Compatible<_Yp>>
	__weak_ptr(__weak_ptr<_Yp, _Lp>&& __r) noexcept
	: _M_ptr(__r.lock().get()), _M_refcount(std::move(__r._M_refcount))
        { __r._M_ptr = nullptr; }

      __weak_ptr&
      operator=(const __weak_ptr& __r) noexcept = default;

      template<typename _Yp>
	_Assignable<_Yp>
	operator=(const __weak_ptr<_Yp, _Lp>& __r) noexcept
	{
	  _M_ptr = __r.lock().get();
	  _M_refcount = __r._M_refcount;
	  return *this;
	}

      template<typename _Yp>
	_Assignable<_Yp>
	operator=(const __shared_ptr<_Yp, _Lp>& __r) noexcept
	{
	  _M_ptr = __r._M_ptr;
	  _M_refcount = __r._M_refcount;
	  return *this;
	}

      __weak_ptr&
      operator=(__weak_ptr&& __r) noexcept
      {
	_M_ptr = __r._M_ptr;
	_M_refcount = std::move(__r._M_refcount);
	__r._M_ptr = nullptr;
	return *this;
      }

      template<typename _Yp>
	_Assignable<_Yp>
	operator=(__weak_ptr<_Yp, _Lp>&& __r) noexcept
	{
	  _M_ptr = __r.lock().get();
	  _M_refcount = std::move(__r._M_refcount);
	  __r._M_ptr = nullptr;
	  return *this;
	}

      __shared_ptr<_Tp, _Lp>
      lock() const noexcept
      { return __shared_ptr<element_type, _Lp>(*this, std::nothrow); }

      long
      use_count() const noexcept
      { return _M_refcount._M_get_use_count(); }

      bool
      expired() const noexcept
      { return _M_refcount._M_get_use_count() == 0; }

      template<typename _Tp1>
	bool
	owner_before(const __shared_ptr<_Tp1, _Lp>& __rhs) const noexcept
	{ return _M_refcount._M_less(__rhs._M_refcount); }

      template<typename _Tp1>
	bool
	owner_before(const __weak_ptr<_Tp1, _Lp>& __rhs) const noexcept
	{ return _M_refcount._M_less(__rhs._M_refcount); }

      void
      reset() noexcept
      { __weak_ptr().swap(*this); }

      void
      swap(__weak_ptr& __s) noexcept
      {
	std::swap(_M_ptr, __s._M_ptr);
	_M_refcount._M_swap(__s._M_refcount);
      }

    private:
      // Used by __enable_shared_from_this.
      void
      _M_assign(_Tp* __ptr, const __shared_count<_Lp>& __refcount) noexcept
      {
	if (use_count() == 0)
	  {
	    _M_ptr = __ptr;
	    _M_refcount = __refcount;
	  }
      }

      template<typename _Tp1, _Lock_policy _Lp1> friend class __shared_ptr;
      template<typename _Tp1, _Lock_policy _Lp1> friend class __weak_ptr;
      friend class __enable_shared_from_this<_Tp, _Lp>;
      friend class enable_shared_from_this<_Tp>;

      element_type*	 _M_ptr;         // Contained pointer.
      __weak_count<_Lp>  _M_refcount;    // Reference counter.
    };
