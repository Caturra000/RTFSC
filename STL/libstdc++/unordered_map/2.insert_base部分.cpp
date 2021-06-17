  /**
   *  Primary class template _Insert_base.
   *
   *  Defines @c insert member functions appropriate to all _Hashtables.
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Insert_base
    {
    protected:
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey,
				     _Equal, _H1, _H2, _Hash,
				     _RehashPolicy, _Traits>;

      using __hashtable_base = _Hashtable_base<_Key, _Value, _ExtractKey,
					       _Equal, _H1, _H2, _Hash,
					       _Traits>;

      using value_type = typename __hashtable_base::value_type;
      using iterator = typename __hashtable_base::iterator;
      using const_iterator =  typename __hashtable_base::const_iterator;
      using size_type = typename __hashtable_base::size_type;

      using __unique_keys = typename __hashtable_base::__unique_keys;
      using __ireturn_type = typename __hashtable_base::__ireturn_type;
      using __node_type = _Hash_node<_Value, _Traits::__hash_cached::value>;
      using __node_alloc_type = __alloc_rebind<_Alloc, __node_type>;
      using __node_gen_type = _AllocNode<__node_alloc_type>;

      __hashtable&
      _M_conjure_hashtable()
      { return *(static_cast<__hashtable*>(this)); }

      template<typename _InputIterator, typename _NodeGetter>
	void
	_M_insert_range(_InputIterator __first, _InputIterator __last,
			const _NodeGetter&, true_type);

      template<typename _InputIterator, typename _NodeGetter>
	void
	_M_insert_range(_InputIterator __first, _InputIterator __last,
			const _NodeGetter&, false_type);

    public:
      __ireturn_type
      insert(const value_type& __v)
      {
	__hashtable& __h = _M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(__v, __node_gen, __unique_keys());
      }

      iterator
      insert(const_iterator __hint, const value_type& __v)
      {
	__hashtable& __h = _M_conjure_hashtable();
	__node_gen_type __node_gen(__h);	
	return __h._M_insert(__hint, __v, __node_gen, __unique_keys());
      }

      void
      insert(initializer_list<value_type> __l)
      { this->insert(__l.begin(), __l.end()); }

      template<typename _InputIterator>
	void
	insert(_InputIterator __first, _InputIterator __last)
	{
	  __hashtable& __h = _M_conjure_hashtable();
	  __node_gen_type __node_gen(__h);
	  return _M_insert_range(__first, __last, __node_gen, __unique_keys());
	}
    };

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    template<typename _InputIterator, typename _NodeGetter>
      void
      _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		    _RehashPolicy, _Traits>::
      _M_insert_range(_InputIterator __first, _InputIterator __last,
		      const _NodeGetter& __node_gen, true_type)
      {
	size_type __n_elt = __detail::__distance_fw(__first, __last);
	if (__n_elt == 0)
	  return;

	__hashtable& __h = _M_conjure_hashtable();
	for (; __first != __last; ++__first)
	  {
	    if (__h._M_insert(*__first, __node_gen, __unique_keys(),
			      __n_elt).second)
	      __n_elt = 1;
	    else if (__n_elt != 1)
	      --__n_elt;
	  }
      }

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    template<typename _InputIterator, typename _NodeGetter>
      void
      _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		    _RehashPolicy, _Traits>::
      _M_insert_range(_InputIterator __first, _InputIterator __last,
		      const _NodeGetter& __node_gen, false_type)
      {
	using __rehash_type = typename __hashtable::__rehash_type;
	using __rehash_state = typename __hashtable::__rehash_state;
	using pair_type = std::pair<bool, std::size_t>;

	size_type __n_elt = __detail::__distance_fw(__first, __last);
	if (__n_elt == 0)
	  return;

	__hashtable& __h = _M_conjure_hashtable();
	__rehash_type& __rehash = __h._M_rehash_policy;
	const __rehash_state& __saved_state = __rehash._M_state();
	pair_type __do_rehash = __rehash._M_need_rehash(__h._M_bucket_count,
							__h._M_element_count,
							__n_elt);

	if (__do_rehash.first)
	  __h._M_rehash(__do_rehash.second, __saved_state);

	for (; __first != __last; ++__first)
	  __h._M_insert(*__first, __node_gen, __unique_keys());
      }

  /**
   *  Primary class template _Insert.
   *
   *  Defines @c insert member functions that depend on _Hashtable policies,
   *  via partial specializations.
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   bool _Constant_iterators = _Traits::__constant_iterators::value>
    struct _Insert;

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Insert<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		   _RehashPolicy, _Traits, true>
    : public _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			   _H1, _H2, _Hash, _RehashPolicy, _Traits>
    {
      using __base_type = _Insert_base<_Key, _Value, _Alloc, _ExtractKey,
					_Equal, _H1, _H2, _Hash,
					_RehashPolicy, _Traits>;

      using __hashtable_base = _Hashtable_base<_Key, _Value, _ExtractKey,
					       _Equal, _H1, _H2, _Hash,
					       _Traits>;

      using value_type = typename __base_type::value_type;
      using iterator = typename __base_type::iterator;
      using const_iterator =  typename __base_type::const_iterator;

      using __unique_keys = typename __base_type::__unique_keys;
      using __ireturn_type = typename __hashtable_base::__ireturn_type;
      using __hashtable = typename __base_type::__hashtable;
      using __node_gen_type = typename __base_type::__node_gen_type;

      using __base_type::insert;

      __ireturn_type
      insert(value_type&& __v)
      {
	__hashtable& __h = this->_M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(std::move(__v), __node_gen, __unique_keys());
      }

      iterator
      insert(const_iterator __hint, value_type&& __v)
      {
	__hashtable& __h = this->_M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(__hint, std::move(__v), __node_gen,
			     __unique_keys());
      }
    };

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Insert<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		   _RehashPolicy, _Traits, false>
    : public _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			   _H1, _H2, _Hash, _RehashPolicy, _Traits>
    {
      using __base_type = _Insert_base<_Key, _Value, _Alloc, _ExtractKey,
				       _Equal, _H1, _H2, _Hash,
				       _RehashPolicy, _Traits>;
      using value_type = typename __base_type::value_type;
      using iterator = typename __base_type::iterator;
      using const_iterator =  typename __base_type::const_iterator;

      using __unique_keys = typename __base_type::__unique_keys;
      using __hashtable = typename __base_type::__hashtable;
      using __ireturn_type = typename __base_type::__ireturn_type;

      using __base_type::insert;

      template<typename _Pair>
	using __is_cons = std::is_constructible<value_type, _Pair&&>;

      template<typename _Pair>
	using _IFcons = std::enable_if<__is_cons<_Pair>::value>;

      template<typename _Pair>
	using _IFconsp = typename _IFcons<_Pair>::type;

      template<typename _Pair, typename = _IFconsp<_Pair>>
	__ireturn_type
	insert(_Pair&& __v)
	{
	  __hashtable& __h = this->_M_conjure_hashtable();
	  return __h._M_emplace(__unique_keys(), std::forward<_Pair>(__v));
	}

      template<typename _Pair, typename = _IFconsp<_Pair>>
	iterator
	insert(const_iterator __hint, _Pair&& __v)
	{
	  __hashtable& __h = this->_M_conjure_hashtable();
	  return __h._M_emplace(__hint, __unique_keys(),
				std::forward<_Pair>(__v));
	}
   };
