  /**
   *  Primary class template _Local_iterator_base.
   *
   *  Base class for local iterators, used to iterate within a bucket
   *  but not between buckets.
   */
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __cache_hash_code>
    struct _Local_iterator_base;




  /// Partial specialization used when nodes contain a cached hash code.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Local_iterator_base<_Key, _Value, _ExtractKey,
				_H1, _H2, _Hash, true>
    : private _Hashtable_ebo_helper<0, _H2>
    {
    protected:
      using __base_type = _Hashtable_ebo_helper<0, _H2>;
      using __hash_code_base = _Hash_code_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, true>;

      _Local_iterator_base() = default;
      _Local_iterator_base(const __hash_code_base& __base,
			   _Hash_node<_Value, true>* __p,
			   std::size_t __bkt, std::size_t __bkt_count)
      : __base_type(__base._M_h2()),
	_M_cur(__p), _M_bucket(__bkt), _M_bucket_count(__bkt_count) { }

      void
      _M_incr()
      {
	_M_cur = _M_cur->_M_next();
	if (_M_cur)
	  {
	    std::size_t __bkt
	      = __base_type::_S_get(*this)(_M_cur->_M_hash_code,
					   _M_bucket_count);
	    if (__bkt != _M_bucket)
	      _M_cur = nullptr;
	  }
      }

      _Hash_node<_Value, true>*  _M_cur;
      std::size_t _M_bucket;
      std::size_t _M_bucket_count;

    public:
      const void*
      _M_curr() const { return _M_cur; }  // for equality ops

      std::size_t
      _M_get_bucket() const { return _M_bucket; }  // for debug mode
    };

  // Uninitialized storage for a _Hash_code_base.
  // This type is DefaultConstructible and Assignable even if the
  // _Hash_code_base type isn't, so that _Local_iterator_base<..., false>
  // can be DefaultConstructible and Assignable.
  template<typename _Tp, bool _IsEmpty = std::is_empty<_Tp>::value>
    struct _Hash_code_storage
    {
      __gnu_cxx::__aligned_buffer<_Tp> _M_storage;

      _Tp*
      _M_h() { return _M_storage._M_ptr(); }

      const _Tp*
      _M_h() const { return _M_storage._M_ptr(); }
    };

  // Empty partial specialization for empty _Hash_code_base types.
  template<typename _Tp>
    struct _Hash_code_storage<_Tp, true>
    {
      static_assert( std::is_empty<_Tp>::value, "Type must be empty" );

      // As _Tp is an empty type there will be no bytes written/read through
      // the cast pointer, so no strict-aliasing violation.
      _Tp*
      _M_h() { return reinterpret_cast<_Tp*>(this); }

      const _Tp*
      _M_h() const { return reinterpret_cast<const _Tp*>(this); }
    };

  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    using __hash_code_for_local_iter
      = _Hash_code_storage<_Hash_code_base<_Key, _Value, _ExtractKey,
					   _H1, _H2, _Hash, false>>;

  // Partial specialization used when hash codes are not cached
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Local_iterator_base<_Key, _Value, _ExtractKey,
				_H1, _H2, _Hash, false>
    : __hash_code_for_local_iter<_Key, _Value, _ExtractKey, _H1, _H2, _Hash>
    {
    protected:
      using __hash_code_base = _Hash_code_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, false>;

      _Local_iterator_base() : _M_bucket_count(-1) { }

      _Local_iterator_base(const __hash_code_base& __base,
			   _Hash_node<_Value, false>* __p,
			   std::size_t __bkt, std::size_t __bkt_count)
      : _M_cur(__p), _M_bucket(__bkt), _M_bucket_count(__bkt_count)
      { _M_init(__base); }

      ~_Local_iterator_base()
      {
	if (_M_bucket_count != -1)
	  _M_destroy();
      }

      _Local_iterator_base(const _Local_iterator_base& __iter)
      : _M_cur(__iter._M_cur), _M_bucket(__iter._M_bucket),
        _M_bucket_count(__iter._M_bucket_count)
      {
	if (_M_bucket_count != -1)
	  _M_init(*__iter._M_h());
      }

      _Local_iterator_base&
      operator=(const _Local_iterator_base& __iter)
      {
	if (_M_bucket_count != -1)
	  _M_destroy();
	_M_cur = __iter._M_cur;
	_M_bucket = __iter._M_bucket;
	_M_bucket_count = __iter._M_bucket_count;
	if (_M_bucket_count != -1)
	  _M_init(*__iter._M_h());
	return *this;
      }

      void
      _M_incr()
      {
	_M_cur = _M_cur->_M_next();
	if (_M_cur)
	  {
	    std::size_t __bkt = this->_M_h()->_M_bucket_index(_M_cur,
							      _M_bucket_count);
	    if (__bkt != _M_bucket)
	      _M_cur = nullptr;
	  }
      }

      _Hash_node<_Value, false>*  _M_cur;
      std::size_t _M_bucket;
      std::size_t _M_bucket_count;

      void
      _M_init(const __hash_code_base& __base)
      { ::new(this->_M_h()) __hash_code_base(__base); }

      void
      _M_destroy() { this->_M_h()->~__hash_code_base(); }

    public:
      const void*
      _M_curr() const { return _M_cur; }  // for equality ops and debug mode

      std::size_t
      _M_get_bucket() const { return _M_bucket; }  // for debug mode
    };

  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash, bool __cache>
    inline bool
    operator==(const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __x,
	       const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __y)
    { return __x._M_curr() == __y._M_curr(); }

  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash, bool __cache>
    inline bool
    operator!=(const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __x,
	       const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __y)
    { return __x._M_curr() != __y._M_curr(); }

  /// local iterators
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __constant_iterators, bool __cache>
    struct _Local_iterator
    : public _Local_iterator_base<_Key, _Value, _ExtractKey,
				  _H1, _H2, _Hash, __cache>
    {
    private:
      using __base_type = _Local_iterator_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, __cache>;
      using __hash_code_base = typename __base_type::__hash_code_base;
    public:
      typedef _Value					value_type;
      typedef typename std::conditional<__constant_iterators,
					const _Value*, _Value*>::type
						       pointer;
      typedef typename std::conditional<__constant_iterators,
					const _Value&, _Value&>::type
						       reference;
      typedef std::ptrdiff_t				difference_type;
      typedef std::forward_iterator_tag			iterator_category;

      _Local_iterator() = default;

      _Local_iterator(const __hash_code_base& __base,
		      _Hash_node<_Value, __cache>* __p,
		      std::size_t __bkt, std::size_t __bkt_count)
	: __base_type(__base, __p, __bkt, __bkt_count)
      { }

      reference
      operator*() const
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const
      { return this->_M_cur->_M_valptr(); }

      _Local_iterator&
      operator++()
      {
	this->_M_incr();
	return *this;
      }

      _Local_iterator
      operator++(int)
      {
	_Local_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  /// local const_iterators
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __constant_iterators, bool __cache>
    struct _Local_const_iterator
    : public _Local_iterator_base<_Key, _Value, _ExtractKey,
				  _H1, _H2, _Hash, __cache>
    {
    private:
      using __base_type = _Local_iterator_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, __cache>;
      using __hash_code_base = typename __base_type::__hash_code_base;

    public:
      typedef _Value					value_type;
      typedef const _Value*				pointer;
      typedef const _Value&				reference;
      typedef std::ptrdiff_t				difference_type;
      typedef std::forward_iterator_tag			iterator_category;

      _Local_const_iterator() = default;

      _Local_const_iterator(const __hash_code_base& __base,
			    _Hash_node<_Value, __cache>* __p,
			    std::size_t __bkt, std::size_t __bkt_count)
	: __base_type(__base, __p, __bkt, __bkt_count)
      { }

      _Local_const_iterator(const _Local_iterator<_Key, _Value, _ExtractKey,
						  _H1, _H2, _Hash,
						  __constant_iterators,
						  __cache>& __x)
	: __base_type(__x)
      { }

      reference
      operator*() const
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const
      { return this->_M_cur->_M_valptr(); }

      _Local_const_iterator&
      operator++()
      {
	this->_M_incr();
	return *this;
      }

      _Local_const_iterator
      operator++(int)
      {
	_Local_const_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };
