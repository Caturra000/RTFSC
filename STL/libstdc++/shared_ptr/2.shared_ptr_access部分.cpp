  // Define operator* and operator-> for shared_ptr<T>.
  template<typename _Tp, _Lock_policy _Lp,
	   bool = is_array<_Tp>::value, bool = is_void<_Tp>::value>
    class __shared_ptr_access
    {
    public:
      using element_type = _Tp;

      element_type&
      operator*() const noexcept
      {
	__glibcxx_assert(_M_get() != nullptr);
	return *_M_get();
      }

      element_type*
      operator->() const noexcept
      {
	_GLIBCXX_DEBUG_PEDASSERT(_M_get() != nullptr);
	return _M_get();
      }

    private:
      element_type*
      _M_get() const noexcept
      { return static_cast<const __shared_ptr<_Tp, _Lp>*>(this)->get(); }
    };

  // Define operator-> for shared_ptr<cv void>.
  template<typename _Tp, _Lock_policy _Lp>
    class __shared_ptr_access<_Tp, _Lp, false, true>
    {
    public:
      using element_type = _Tp;

      element_type*
      operator->() const noexcept
      {
	auto __ptr = static_cast<const __shared_ptr<_Tp, _Lp>*>(this)->get();
	_GLIBCXX_DEBUG_PEDASSERT(__ptr != nullptr);
	return __ptr;
      }
    };

  // Define operator[] for shared_ptr<T[]> and shared_ptr<T[N]>.
  template<typename _Tp, _Lock_policy _Lp>
    class __shared_ptr_access<_Tp, _Lp, true, false>
    {
    public:
      using element_type = typename remove_extent<_Tp>::type;

#if __cplusplus <= 201402L
      [[__deprecated__("shared_ptr<T[]>::operator* is absent from C++17")]]
      element_type&
      operator*() const noexcept
      {
	__glibcxx_assert(_M_get() != nullptr);
	return *_M_get();
      }

      [[__deprecated__("shared_ptr<T[]>::operator-> is absent from C++17")]]
      element_type*
      operator->() const noexcept
      {
	_GLIBCXX_DEBUG_PEDASSERT(_M_get() != nullptr);
	return _M_get();
      }
#endif

      element_type&
      operator[](ptrdiff_t __i) const
      {
	__glibcxx_assert(_M_get() != nullptr);
	__glibcxx_assert(!extent<_Tp>::value || __i < extent<_Tp>::value);
	return _M_get()[__i];
      }

    private:
      element_type*
      _M_get() const noexcept
      { return static_cast<const __shared_ptr<_Tp, _Lp>*>(this)->get(); }
    };
