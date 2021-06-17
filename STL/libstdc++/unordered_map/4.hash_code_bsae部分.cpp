  /**
   *  Primary class template _Hash_code_base.
   *
   *  Encapsulates two policy issues that aren't quite orthogonal.
   *   (1) the difference between using a ranged hash function and using
   *       the combination of a hash function and a range-hashing function.
   *       In the former case we don't have such things as hash codes, so
   *       we have a dummy type as placeholder.
   *   (2) Whether or not we cache hash codes.  Caching hash codes is
   *       meaningless if we have a ranged hash function.
   *
   *  We also put the key extraction objects here, for convenience.
   *  Each specialization derives from one or more of the template
   *  parameters to benefit from Ebo. This is important as this type
   *  is inherited in some cases by the _Local_iterator_base type used
   *  to implement local_iterator and const_local_iterator. As with
   *  any iterator type we prefer to make it as small as possible.
   *
   *  Primary template is unused except as a hook for specializations.
   */
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __cache_hash_code>
    struct _Hash_code_base;

  /// Specialization: ranged hash function, no caching hash codes.  H1
  /// and H2 are provided but ignored.  We define a dummy hash code type.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2, _Hash, false>
    : private _Hashtable_ebo_helper<0, _ExtractKey>,
      private _Hashtable_ebo_helper<1, _Hash>
    {
    private:
      using __ebo_extract_key = _Hashtable_ebo_helper<0, _ExtractKey>;
      using __ebo_hash = _Hashtable_ebo_helper<1, _Hash>;

    protected:
      typedef void* 					__hash_code;
      typedef _Hash_node<_Value, false>			__node_type;

      // We need the default constructor for the local iterators and _Hashtable
      // default constructor.
      _Hash_code_base() = default;

      _Hash_code_base(const _ExtractKey& __ex, const _H1&, const _H2&,
		      const _Hash& __h)
      : __ebo_extract_key(__ex), __ebo_hash(__h) { }

      __hash_code
      _M_hash_code(const _Key& __key) const
      { return 0; }

      std::size_t
      _M_bucket_index(const _Key& __k, __hash_code, std::size_t __n) const
      { return _M_ranged_hash()(__k, __n); }

      std::size_t
      _M_bucket_index(const __node_type* __p, std::size_t __n) const
	noexcept( noexcept(declval<const _Hash&>()(declval<const _Key&>(),
						   (std::size_t)0)) )
      { return _M_ranged_hash()(_M_extract()(__p->_M_v()), __n); }

      void
      _M_store_code(__node_type*, __hash_code) const
      { }

      void
      _M_copy_code(__node_type*, const __node_type*) const
      { }

      void
      _M_swap(_Hash_code_base& __x)
      {
	std::swap(_M_extract(), __x._M_extract());
	std::swap(_M_ranged_hash(), __x._M_ranged_hash());
      }

      const _ExtractKey&
      _M_extract() const { return __ebo_extract_key::_S_cget(*this); }

      _ExtractKey&
      _M_extract() { return __ebo_extract_key::_S_get(*this); }

      const _Hash&
      _M_ranged_hash() const { return __ebo_hash::_S_cget(*this); }

      _Hash&
      _M_ranged_hash() { return __ebo_hash::_S_get(*this); }
    };

  // No specialization for ranged hash function while caching hash codes.
  // That combination is meaningless, and trying to do it is an error.

  /// Specialization: ranged hash function, cache hash codes.  This
  /// combination is meaningless, so we provide only a declaration
  /// and no definition.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2, _Hash, true>;

  /// Specialization: hash function and range-hashing function, no
  /// caching of hash codes.
  /// Provides typedef and accessor required by C++ 11.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2,
			   _Default_ranged_hash, false>
    : private _Hashtable_ebo_helper<0, _ExtractKey>,
      private _Hashtable_ebo_helper<1, _H1>,
      private _Hashtable_ebo_helper<2, _H2>
    {
    private:
      using __ebo_extract_key = _Hashtable_ebo_helper<0, _ExtractKey>;
      using __ebo_h1 = _Hashtable_ebo_helper<1, _H1>;
      using __ebo_h2 = _Hashtable_ebo_helper<2, _H2>;

      // Gives the local iterator implementation access to _M_bucket_index().
      friend struct _Local_iterator_base<_Key, _Value, _ExtractKey, _H1, _H2,
					 _Default_ranged_hash, false>;

    public:
      typedef _H1 					hasher;

      hasher
      hash_function() const
      { return _M_h1(); }

    protected:
      typedef std::size_t 				__hash_code;
      typedef _Hash_node<_Value, false>			__node_type;

      // We need the default constructor for the local iterators and _Hashtable
      // default constructor.
      _Hash_code_base() = default;

      _Hash_code_base(const _ExtractKey& __ex,
		      const _H1& __h1, const _H2& __h2,
		      const _Default_ranged_hash&)
      : __ebo_extract_key(__ex), __ebo_h1(__h1), __ebo_h2(__h2) { }

      __hash_code
      _M_hash_code(const _Key& __k) const
      {
	static_assert(__is_invocable<const _H1&, const _Key&>{},
	    "hash function must be invocable with an argument of key type");
	return _M_h1()(__k);
      }

      std::size_t
      _M_bucket_index(const _Key&, __hash_code __c, std::size_t __n) const
      { return _M_h2()(__c, __n); }

      std::size_t
      _M_bucket_index(const __node_type* __p, std::size_t __n) const
	noexcept( noexcept(declval<const _H1&>()(declval<const _Key&>()))
		  && noexcept(declval<const _H2&>()((__hash_code)0,
						    (std::size_t)0)) )
      { return _M_h2()(_M_h1()(_M_extract()(__p->_M_v())), __n); }

      void
      _M_store_code(__node_type*, __hash_code) const
      { }

      void
      _M_copy_code(__node_type*, const __node_type*) const
      { }

      void
      _M_swap(_Hash_code_base& __x)
      {
	std::swap(_M_extract(), __x._M_extract());
	std::swap(_M_h1(), __x._M_h1());
	std::swap(_M_h2(), __x._M_h2());
      }

      const _ExtractKey&
      _M_extract() const { return __ebo_extract_key::_S_cget(*this); }

      _ExtractKey&
      _M_extract() { return __ebo_extract_key::_S_get(*this); }

      const _H1&
      _M_h1() const { return __ebo_h1::_S_cget(*this); }

      _H1&
      _M_h1() { return __ebo_h1::_S_get(*this); }

      const _H2&
      _M_h2() const { return __ebo_h2::_S_cget(*this); }

      _H2&
      _M_h2() { return __ebo_h2::_S_get(*this); }
    };

  /// Specialization: hash function and range-hashing function,
  /// caching hash codes.  H is provided but ignored.  Provides
  /// typedef and accessor required by C++ 11.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2,
			   _Default_ranged_hash, true>
    : private _Hashtable_ebo_helper<0, _ExtractKey>,
      private _Hashtable_ebo_helper<1, _H1>,
      private _Hashtable_ebo_helper<2, _H2>
    {
    private:
      // Gives the local iterator implementation access to _M_h2().
      friend struct _Local_iterator_base<_Key, _Value, _ExtractKey, _H1, _H2,
					 _Default_ranged_hash, true>;

      using __ebo_extract_key = _Hashtable_ebo_helper<0, _ExtractKey>;
      using __ebo_h1 = _Hashtable_ebo_helper<1, _H1>;
      using __ebo_h2 = _Hashtable_ebo_helper<2, _H2>;

    public:
      typedef _H1 					hasher;

      hasher
      hash_function() const
      { return _M_h1(); }

    protected:
      typedef std::size_t 				__hash_code;
      typedef _Hash_node<_Value, true>			__node_type;

      // We need the default constructor for _Hashtable default constructor.
      _Hash_code_base() = default;
      _Hash_code_base(const _ExtractKey& __ex,
		      const _H1& __h1, const _H2& __h2,
		      const _Default_ranged_hash&)
      : __ebo_extract_key(__ex), __ebo_h1(__h1), __ebo_h2(__h2) { }

      __hash_code
      _M_hash_code(const _Key& __k) const
      {
	static_assert(__is_invocable<const _H1&, const _Key&>{},
	    "hash function must be invocable with an argument of key type");
	return _M_h1()(__k);
      }

      std::size_t
      _M_bucket_index(const _Key&, __hash_code __c,
		      std::size_t __n) const
      { return _M_h2()(__c, __n); }

      std::size_t
      _M_bucket_index(const __node_type* __p, std::size_t __n) const
	noexcept( noexcept(declval<const _H2&>()((__hash_code)0,
						 (std::size_t)0)) )
      { return _M_h2()(__p->_M_hash_code, __n); }

      void
      _M_store_code(__node_type* __n, __hash_code __c) const
      { __n->_M_hash_code = __c; }

      void
      _M_copy_code(__node_type* __to, const __node_type* __from) const
      { __to->_M_hash_code = __from->_M_hash_code; }

      void
      _M_swap(_Hash_code_base& __x)
      {
	std::swap(_M_extract(), __x._M_extract());
	std::swap(_M_h1(), __x._M_h1());
	std::swap(_M_h2(), __x._M_h2());
      }

      const _ExtractKey&
      _M_extract() const { return __ebo_extract_key::_S_cget(*this); }

      _ExtractKey&
      _M_extract() { return __ebo_extract_key::_S_get(*this); }

      const _H1&
      _M_h1() const { return __ebo_h1::_S_cget(*this); }

      _H1&
      _M_h1() { return __ebo_h1::_S_get(*this); }

      const _H2&
      _M_h2() const { return __ebo_h2::_S_cget(*this); }

      _H2&
      _M_h2() { return __ebo_h2::_S_get(*this); }
    };
