  /**
   *  @brief  A smart pointer with reference-counted copy semantics.
   *
   *  The object pointed to is deleted when the last shared_ptr pointing to
   *  it is destroyed or reset.
  */
  template<typename _Tp>
    class shared_ptr : public __shared_ptr<_Tp>
    {
      template<typename... _Args>
	using _Constructible = typename enable_if<
	  is_constructible<__shared_ptr<_Tp>, _Args...>::value
	>::type;

      template<typename _Arg>
	using _Assignable = typename enable_if<
	  is_assignable<__shared_ptr<_Tp>&, _Arg>::value, shared_ptr&
	>::type;

    public:

      using element_type = typename __shared_ptr<_Tp>::element_type;

#if __cplusplus > 201402L
# define __cpp_lib_shared_ptr_weak_type 201606
      using weak_type = weak_ptr<_Tp>;
#endif
      /**
       *  @brief  Construct an empty %shared_ptr.
       *  @post   use_count()==0 && get()==0
       */
      constexpr shared_ptr() noexcept : __shared_ptr<_Tp>() { }

      shared_ptr(const shared_ptr&) noexcept = default;

      template<typename _Yp, typename = _Constructible<_Yp*>>
	explicit
	shared_ptr(_Yp* __p) : __shared_ptr<_Tp>(__p) { }

      template<typename _Yp, typename _Deleter,
	       typename = _Constructible<_Yp*, _Deleter>>
	shared_ptr(_Yp* __p, _Deleter __d)
        : __shared_ptr<_Tp>(__p, std::move(__d)) { }

      template<typename _Deleter>
	shared_ptr(nullptr_t __p, _Deleter __d)
        : __shared_ptr<_Tp>(__p, std::move(__d)) { }

      template<typename _Yp, typename _Deleter, typename _Alloc,
	       typename = _Constructible<_Yp*, _Deleter, _Alloc>>
	shared_ptr(_Yp* __p, _Deleter __d, _Alloc __a)
	: __shared_ptr<_Tp>(__p, std::move(__d), std::move(__a)) { }

      template<typename _Deleter, typename _Alloc>
	shared_ptr(nullptr_t __p, _Deleter __d, _Alloc __a)
	: __shared_ptr<_Tp>(__p, std::move(__d), std::move(__a)) { }

      // Aliasing constructor

      template<typename _Yp>
	shared_ptr(const shared_ptr<_Yp>& __r, element_type* __p) noexcept
	: __shared_ptr<_Tp>(__r, __p) { }

      template<typename _Yp,
	       typename = _Constructible<const shared_ptr<_Yp>&>>
	shared_ptr(const shared_ptr<_Yp>& __r) noexcept
        : __shared_ptr<_Tp>(__r) { }

      shared_ptr(shared_ptr&& __r) noexcept
      : __shared_ptr<_Tp>(std::move(__r)) { }

      template<typename _Yp, typename = _Constructible<shared_ptr<_Yp>>>
	shared_ptr(shared_ptr<_Yp>&& __r) noexcept
	: __shared_ptr<_Tp>(std::move(__r)) { }

      template<typename _Yp, typename = _Constructible<const weak_ptr<_Yp>&>>
	explicit shared_ptr(const weak_ptr<_Yp>& __r)
	: __shared_ptr<_Tp>(__r) { }

#if _GLIBCXX_USE_DEPRECATED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      template<typename _Yp, typename = _Constructible<auto_ptr<_Yp>>>
	shared_ptr(auto_ptr<_Yp>&& __r);
#pragma GCC diagnostic pop
#endif

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 2399. shared_ptr's constructor from unique_ptr should be constrained
      template<typename _Yp, typename _Del,
	       typename = _Constructible<unique_ptr<_Yp, _Del>>>
	shared_ptr(unique_ptr<_Yp, _Del>&& __r)
	: __shared_ptr<_Tp>(std::move(__r)) { }

#if __cplusplus <= 201402L && _GLIBCXX_USE_DEPRECATED
      // This non-standard constructor exists to support conversions that
      // were possible in C++11 and C++14 but are ill-formed in C++17.
      // If an exception is thrown this constructor has no effect.
      template<typename _Yp, typename _Del,
		_Constructible<unique_ptr<_Yp, _Del>, __sp_array_delete>* = 0>
	shared_ptr(unique_ptr<_Yp, _Del>&& __r)
	: __shared_ptr<_Tp>(std::move(__r), __sp_array_delete()) { }
#endif

      /**
       *  @brief  Construct an empty %shared_ptr.
       *  @post   use_count() == 0 && get() == nullptr
       */
      constexpr shared_ptr(nullptr_t) noexcept : shared_ptr() { }

      shared_ptr& operator=(const shared_ptr&) noexcept = default;

      template<typename _Yp>
	_Assignable<const shared_ptr<_Yp>&>
	operator=(const shared_ptr<_Yp>& __r) noexcept
	{
	  this->__shared_ptr<_Tp>::operator=(__r);
	  return *this;
	}

#if _GLIBCXX_USE_DEPRECATED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      template<typename _Yp>
	_Assignable<auto_ptr<_Yp>>
	operator=(auto_ptr<_Yp>&& __r)
	{
	  this->__shared_ptr<_Tp>::operator=(std::move(__r));
	  return *this;
	}
#pragma GCC diagnostic pop
#endif

      shared_ptr&
      operator=(shared_ptr&& __r) noexcept
      {
	this->__shared_ptr<_Tp>::operator=(std::move(__r));
	return *this;
      }

      template<class _Yp>
	_Assignable<shared_ptr<_Yp>>
	operator=(shared_ptr<_Yp>&& __r) noexcept
	{
	  this->__shared_ptr<_Tp>::operator=(std::move(__r));
	  return *this;
	}

      template<typename _Yp, typename _Del>
	_Assignable<unique_ptr<_Yp, _Del>>
	operator=(unique_ptr<_Yp, _Del>&& __r)
	{
	  this->__shared_ptr<_Tp>::operator=(std::move(__r));
	  return *this;
	}

    private:
      // This constructor is non-standard, it is used by allocate_shared.
      template<typename _Alloc, typename... _Args>
	shared_ptr(_Sp_alloc_shared_tag<_Alloc> __tag, _Args&&... __args)
	: __shared_ptr<_Tp>(__tag, std::forward<_Args>(__args)...)
	{ }

      template<typename _Yp, typename _Alloc, typename... _Args>
	friend shared_ptr<_Yp>
	allocate_shared(const _Alloc& __a, _Args&&... __args);

      // This constructor is non-standard, it is used by weak_ptr::lock().
      shared_ptr(const weak_ptr<_Tp>& __r, std::nothrow_t)
      : __shared_ptr<_Tp>(__r, std::nothrow) { }

      friend class weak_ptr<_Tp>;
    };

