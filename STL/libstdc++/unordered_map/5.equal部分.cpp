  /**
   *  Primary class template _Equal_helper.
   *
   */
  template <typename _Key, typename _Value, typename _ExtractKey,
	    typename _Equal, typename _HashCodeType,
	    bool __cache_hash_code>
  struct _Equal_helper;

  /// Specialization.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _Equal, typename _HashCodeType>
  struct _Equal_helper<_Key, _Value, _ExtractKey, _Equal, _HashCodeType, true>
  {
    static bool
    _S_equals(const _Equal& __eq, const _ExtractKey& __extract,
	      const _Key& __k, _HashCodeType __c, _Hash_node<_Value, true>* __n)
    { return __c == __n->_M_hash_code && __eq(__k, __extract(__n->_M_v())); }
  };

  /// Specialization.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _Equal, typename _HashCodeType>
  struct _Equal_helper<_Key, _Value, _ExtractKey, _Equal, _HashCodeType, false>
  {
    static bool
    _S_equals(const _Equal& __eq, const _ExtractKey& __extract,
	      const _Key& __k, _HashCodeType, _Hash_node<_Value, false>* __n)
    { return __eq(__k, __extract(__n->_M_v())); }
  };










  // See std::is_permutation in N3068.
  template<typename _Uiterator>
    bool
    _Equality_base::
    _S_is_permutation(_Uiterator __first1, _Uiterator __last1,
		      _Uiterator __first2)
    {
      for (; __first1 != __last1; ++__first1, ++__first2)
	if (!(*__first1 == *__first2))
	  break;

      if (__first1 == __last1)
	return true;

      _Uiterator __last2 = __first2;
      std::advance(__last2, std::distance(__first1, __last1));

      for (_Uiterator __it1 = __first1; __it1 != __last1; ++__it1)
	{
	  _Uiterator __tmp =  __first1;
	  while (__tmp != __it1 && !bool(*__tmp == *__it1))
	    ++__tmp;

	  // We've seen this one before.
	  if (__tmp != __it1)
	    continue;

	  std::ptrdiff_t __n2 = 0;
	  for (__tmp = __first2; __tmp != __last2; ++__tmp)
	    if (*__tmp == *__it1)
	      ++__n2;

	  if (!__n2)
	    return false;

	  std::ptrdiff_t __n1 = 0;
	  for (__tmp = __it1; __tmp != __last1; ++__tmp)
	    if (*__tmp == *__it1)
	      ++__n1;

	  if (__n1 != __n2)
	    return false;
	}
      return true;
    }

  /**
   *  Primary class template  _Equality.
   *
   *  This is for implementing equality comparison for unordered
   *  containers, per N3068, by John Lakos and Pablo Halpern.
   *  Algorithmically, we follow closely the reference implementations
   *  therein.
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   bool _Unique_keys = _Traits::__unique_keys::value>
    struct _Equality;

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
    {
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey, _Equal,
				     _H1, _H2, _Hash, _RehashPolicy, _Traits>;

      bool
      _M_equal(const __hashtable&) const;
    };

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    bool
    _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    _M_equal(const __hashtable& __other) const
    {
      const __hashtable* __this = static_cast<const __hashtable*>(this);

      if (__this->size() != __other.size())
	return false;

      for (auto __itx = __this->begin(); __itx != __this->end(); ++__itx)
	{
	  const auto __ity = __other.find(_ExtractKey()(*__itx));
	  if (__ity == __other.end() || !bool(*__ity == *__itx))
	    return false;
	}
      return true;
    }

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, false>
    : public _Equality_base
    {
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey, _Equal,
				     _H1, _H2, _Hash, _RehashPolicy, _Traits>;

      bool
      _M_equal(const __hashtable&) const;
    };

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    bool
    _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, false>::
    _M_equal(const __hashtable& __other) const
    {
      const __hashtable* __this = static_cast<const __hashtable*>(this);

      if (__this->size() != __other.size())
	return false;

      for (auto __itx = __this->begin(); __itx != __this->end();)
	{
	  const auto __xrange = __this->equal_range(_ExtractKey()(*__itx));
	  const auto __yrange = __other.equal_range(_ExtractKey()(*__itx));

	  if (std::distance(__xrange.first, __xrange.second)
	      != std::distance(__yrange.first, __yrange.second))
	    return false;

	  if (!_S_is_permutation(__xrange.first, __xrange.second,
				 __yrange.first))
	    return false;

	  __itx = __xrange.second;
	}
      return true;
    }
