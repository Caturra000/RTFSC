
  template<typename _Policy>
    using __has_load_factor = typename _Policy::__has_load_factor;

  /**
   *  Primary class template  _Rehash_base.
   *
   *  Give hashtable the max_load_factor functions and reserve iff the
   *  rehash policy supports it.
  */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   typename =
	     __detected_or_t<std::false_type, __has_load_factor, _RehashPolicy>>
    struct _Rehash_base;

  /// Specialization when rehash policy doesn't provide load factor management.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Rehash_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
		      _H1, _H2, _Hash, _RehashPolicy, _Traits,
		      std::false_type>
    {
    };

  /// Specialization when rehash policy provide load factor management.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Rehash_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			_H1, _H2, _Hash, _RehashPolicy, _Traits,
			std::true_type>
    {
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey,
				     _Equal, _H1, _H2, _Hash,
				     _RehashPolicy, _Traits>;

      float
      max_load_factor() const noexcept
      {
	const __hashtable* __this = static_cast<const __hashtable*>(this);
	return __this->__rehash_policy().max_load_factor();
      }

      void
      max_load_factor(float __z)
      {
	__hashtable* __this = static_cast<__hashtable*>(this);
	__this->__rehash_policy(_RehashPolicy(__z));
      }

      void
      reserve(std::size_t __n)
      {
	__hashtable* __this = static_cast<__hashtable*>(this);
	__this->rehash(__builtin_ceil(__n / max_load_factor()));
      }
    };