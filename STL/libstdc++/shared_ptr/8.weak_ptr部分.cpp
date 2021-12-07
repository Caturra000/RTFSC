  /**
   *  @brief  A smart pointer with weak semantics.
   *
   *  With forwarding constructors and assignment operators.
   */
  template<typename _Tp>
    class weak_ptr : public __weak_ptr<_Tp>
    {
      template<typename _Arg>
	using _Constructible = typename enable_if<
	  is_constructible<__weak_ptr<_Tp>, _Arg>::value
	>::type;

      template<typename _Arg>
	using _Assignable = typename enable_if<
	  is_assignable<__weak_ptr<_Tp>&, _Arg>::value, weak_ptr&
	>::type;

    public:
      constexpr weak_ptr() noexcept = default;

      template<typename _Yp,
	       typename = _Constructible<const shared_ptr<_Yp>&>>
	weak_ptr(const shared_ptr<_Yp>& __r) noexcept
	: __weak_ptr<_Tp>(__r) { }

      weak_ptr(const weak_ptr&) noexcept = default;

      template<typename _Yp, typename = _Constructible<const weak_ptr<_Yp>&>>
	weak_ptr(const weak_ptr<_Yp>& __r) noexcept
	: __weak_ptr<_Tp>(__r) { }

      weak_ptr(weak_ptr&&) noexcept = default;

      template<typename _Yp, typename = _Constructible<weak_ptr<_Yp>>>
	weak_ptr(weak_ptr<_Yp>&& __r) noexcept
	: __weak_ptr<_Tp>(std::move(__r)) { }

      weak_ptr&
      operator=(const weak_ptr& __r) noexcept = default;

      template<typename _Yp>
	_Assignable<const weak_ptr<_Yp>&>
	operator=(const weak_ptr<_Yp>& __r) noexcept
	{
	  this->__weak_ptr<_Tp>::operator=(__r);
	  return *this;
	}

      template<typename _Yp>
	_Assignable<const shared_ptr<_Yp>&>
	operator=(const shared_ptr<_Yp>& __r) noexcept
	{
	  this->__weak_ptr<_Tp>::operator=(__r);
	  return *this;
	}

      weak_ptr&
      operator=(weak_ptr&& __r) noexcept = default;

      template<typename _Yp>
	_Assignable<weak_ptr<_Yp>>
	operator=(weak_ptr<_Yp>&& __r) noexcept
	{
	  this->__weak_ptr<_Tp>::operator=(std::move(__r));
	  return *this;
	}

      shared_ptr<_Tp>
      lock() const noexcept
      { return shared_ptr<_Tp>(*this, std::nothrow); }
    };
