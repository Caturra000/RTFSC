  template<_Lock_policy _Lp>
    class __weak_count
    {
    public:
      constexpr __weak_count() noexcept : _M_pi(nullptr)
      { }

      __weak_count(const __shared_count<_Lp>& __r) noexcept
      : _M_pi(__r._M_pi)
      {
	if (_M_pi != nullptr)
	  _M_pi->_M_weak_add_ref();
      }

      __weak_count(const __weak_count& __r) noexcept
      : _M_pi(__r._M_pi)
      {
	if (_M_pi != nullptr)
	  _M_pi->_M_weak_add_ref();
      }

      __weak_count(__weak_count&& __r) noexcept
      : _M_pi(__r._M_pi)
      { __r._M_pi = nullptr; }

      ~__weak_count() noexcept
      {
	if (_M_pi != nullptr)
	  _M_pi->_M_weak_release();
      }

      __weak_count&
      operator=(const __shared_count<_Lp>& __r) noexcept
      {
	_Sp_counted_base<_Lp>* __tmp = __r._M_pi;
	if (__tmp != nullptr)
	  __tmp->_M_weak_add_ref();
	if (_M_pi != nullptr)
	  _M_pi->_M_weak_release();
	_M_pi = __tmp;
	return *this;
      }

      __weak_count&
      operator=(const __weak_count& __r) noexcept
      {
	_Sp_counted_base<_Lp>* __tmp = __r._M_pi;
	if (__tmp != nullptr)
	  __tmp->_M_weak_add_ref();
	if (_M_pi != nullptr)
	  _M_pi->_M_weak_release();
	_M_pi = __tmp;
	return *this;
      }

      __weak_count&
      operator=(__weak_count&& __r) noexcept
      {
	if (_M_pi != nullptr)
	  _M_pi->_M_weak_release();
	_M_pi = __r._M_pi;
        __r._M_pi = nullptr;
	return *this;
      }

      void
      _M_swap(__weak_count& __r) noexcept
      {
	_Sp_counted_base<_Lp>* __tmp = __r._M_pi;
	__r._M_pi = _M_pi;
	_M_pi = __tmp;
      }

      long
      _M_get_use_count() const noexcept
      { return _M_pi != nullptr ? _M_pi->_M_get_use_count() : 0; }

      bool
      _M_less(const __weak_count& __rhs) const noexcept
      { return std::less<_Sp_counted_base<_Lp>*>()(this->_M_pi, __rhs._M_pi); }

      bool
      _M_less(const __shared_count<_Lp>& __rhs) const noexcept
      { return std::less<_Sp_counted_base<_Lp>*>()(this->_M_pi, __rhs._M_pi); }

      // Friend function injected into enclosing namespace and found by ADL
      friend inline bool
      operator==(const __weak_count& __a, const __weak_count& __b) noexcept
      { return __a._M_pi == __b._M_pi; }

    private:
      friend class __shared_count<_Lp>;

      _Sp_counted_base<_Lp>*  _M_pi;
    };
