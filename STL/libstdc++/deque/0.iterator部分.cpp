// _GLIBCXX_DEQUE_BUF_SIZE大小为512

// _Deque_iterator成员含有
//     _Elt_pointer _M_cur;      // operator*() / operator->() 实际会获取该成员
//     _Elt_pointer _M_first;
//     _Elt_pointer _M_last;     // last - first == _S_buffer_size() == __deque_buf_size(sizeof(_Tp))
//     _Map_pointer _M_node;
// 不考虑特殊情况，可以认为_Elt_pointer就是_Tp*，_Map_pointer是_Tp**
// 可以推断出deque的元素是分开在多段的连续内存中存储的，通过_M_node定位到某一段，其余成员用于描述区间和当前迭代器在区间的位置

// 似乎operator++并没有任何越界保护？

  _GLIBCXX_CONSTEXPR inline size_t
  __deque_buf_size(size_t __size)
  { return (__size < _GLIBCXX_DEQUE_BUF_SIZE
	    ? size_t(_GLIBCXX_DEQUE_BUF_SIZE / __size) : size_t(1)); }


  /**
   *  @brief A deque::iterator.
   *
   *  Quite a bit of intelligence here.  Much of the functionality of
   *  deque is actually passed off to this class.  A deque holds two
   *  of these internally, marking its valid range.  Access to
   *  elements is done as offsets of either of those two, relying on
   *  operator overloading in this class.
   *
   *  All the functions are op overloads except for _M_set_node.
  */
  template<typename _Tp, typename _Ref, typename _Ptr>
    struct _Deque_iterator
    {
#if __cplusplus < 201103L
      typedef _Deque_iterator<_Tp, _Tp&, _Tp*>	     iterator;
      typedef _Deque_iterator<_Tp, const _Tp&, const _Tp*> const_iterator;
      typedef _Tp*					 _Elt_pointer;
      typedef _Tp**					_Map_pointer;
#else
    private:
      template<typename _Up>
	using __ptr_to = typename pointer_traits<_Ptr>::template rebind<_Up>;
      template<typename _CvTp>
	using __iter = _Deque_iterator<_Tp, _CvTp&, __ptr_to<_CvTp>>;
    public:
      typedef __iter<_Tp>		iterator;
      typedef __iter<const _Tp>		const_iterator;
      typedef __ptr_to<_Tp>		_Elt_pointer;
      typedef __ptr_to<_Elt_pointer>	_Map_pointer;
#endif

      static size_t _S_buffer_size() _GLIBCXX_NOEXCEPT
      { return __deque_buf_size(sizeof(_Tp)); }

      typedef std::random_access_iterator_tag	iterator_category;
      typedef _Tp				value_type;
      typedef _Ptr				pointer;
      typedef _Ref				reference;
      typedef size_t				size_type;
      typedef ptrdiff_t				difference_type;
      typedef _Deque_iterator			_Self;

      _Elt_pointer _M_cur;
      _Elt_pointer _M_first;
      _Elt_pointer _M_last;
      _Map_pointer _M_node;

      _Deque_iterator(_Elt_pointer __x, _Map_pointer __y) _GLIBCXX_NOEXCEPT
      : _M_cur(__x), _M_first(*__y),
	_M_last(*__y + _S_buffer_size()), _M_node(__y) { }

      _Deque_iterator() _GLIBCXX_NOEXCEPT
      : _M_cur(), _M_first(), _M_last(), _M_node() { }

#if __cplusplus < 201103L
      // Conversion from iterator to const_iterator.
      _Deque_iterator(const iterator& __x) _GLIBCXX_NOEXCEPT
      : _M_cur(__x._M_cur), _M_first(__x._M_first),
	_M_last(__x._M_last), _M_node(__x._M_node) { }
#else
      // Conversion from iterator to const_iterator.
      template<typename _Iter,
	       typename = _Require<is_same<_Self, const_iterator>,
				   is_same<_Iter, iterator>>>
       _Deque_iterator(const _Iter& __x) noexcept
       : _M_cur(__x._M_cur), _M_first(__x._M_first),
	 _M_last(__x._M_last), _M_node(__x._M_node) { }

      _Deque_iterator(const _Deque_iterator& __x) noexcept
       : _M_cur(__x._M_cur), _M_first(__x._M_first),
	 _M_last(__x._M_last), _M_node(__x._M_node) { }

      _Deque_iterator& operator=(const _Deque_iterator&) = default;
#endif

      iterator
      _M_const_cast() const _GLIBCXX_NOEXCEPT
      { return iterator(_M_cur, _M_node); }

      reference
      operator*() const _GLIBCXX_NOEXCEPT
      { return *_M_cur; }

      pointer
      operator->() const _GLIBCXX_NOEXCEPT
      { return _M_cur; }

      _Self&
      operator++() _GLIBCXX_NOEXCEPT
      {
	++_M_cur;
	if (_M_cur == _M_last)
	  {
	    _M_set_node(_M_node + 1);
	    _M_cur = _M_first;
	  }
	return *this;
      }

      _Self
      operator++(int) _GLIBCXX_NOEXCEPT
      {
	_Self __tmp = *this;
	++*this;
	return __tmp;
      }

      _Self&
      operator--() _GLIBCXX_NOEXCEPT
      {
	if (_M_cur == _M_first)
	  {
	    _M_set_node(_M_node - 1);
	    _M_cur = _M_last;
	  }
	--_M_cur;
	return *this;
      }

      _Self
      operator--(int) _GLIBCXX_NOEXCEPT
      {
	_Self __tmp = *this;
	--*this;
	return __tmp;
      }

      _Self&
      operator+=(difference_type __n) _GLIBCXX_NOEXCEPT
      {
	const difference_type __offset = __n + (_M_cur - _M_first);
	if (__offset >= 0 && __offset < difference_type(_S_buffer_size()))
	  _M_cur += __n;
	else
	  {
	    const difference_type __node_offset =
	      __offset > 0 ? __offset / difference_type(_S_buffer_size())
			   : -difference_type((-__offset - 1)
					      / _S_buffer_size()) - 1;
	    _M_set_node(_M_node + __node_offset);
	    _M_cur = _M_first + (__offset - __node_offset
				 * difference_type(_S_buffer_size()));
	  }
	return *this;
      }

      _Self
      operator+(difference_type __n) const _GLIBCXX_NOEXCEPT
      {
	_Self __tmp = *this;
	return __tmp += __n;
      }

      _Self&
      operator-=(difference_type __n) _GLIBCXX_NOEXCEPT
      { return *this += -__n; }

      _Self
      operator-(difference_type __n) const _GLIBCXX_NOEXCEPT
      {
	_Self __tmp = *this;
	return __tmp -= __n;
      }

      reference
      operator[](difference_type __n) const _GLIBCXX_NOEXCEPT
      { return *(*this + __n); }

      /**
       *  Prepares to traverse new_node.  Sets everything except
       *  _M_cur, which should therefore be set by the caller
       *  immediately afterwards, based on _M_first and _M_last.
       */
      void
      _M_set_node(_Map_pointer __new_node) _GLIBCXX_NOEXCEPT
      {
	_M_node = __new_node;
	_M_first = *__new_node;
	_M_last = _M_first + difference_type(_S_buffer_size());
      }
    };

  // Note: we also provide overloads whose operands are of the same type in
  // order to avoid ambiguous overload resolution when std::rel_ops operators
  // are in scope (for additional details, see libstdc++/3628)
  template<typename _Tp, typename _Ref, typename _Ptr>
    inline bool
    operator==(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	       const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    { return __x._M_cur == __y._M_cur; }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline bool
    operator==(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	       const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    { return __x._M_cur == __y._M_cur; }

  template<typename _Tp, typename _Ref, typename _Ptr>
    inline bool
    operator!=(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	       const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    { return !(__x == __y); }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline bool
    operator!=(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	       const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    { return !(__x == __y); }

  template<typename _Tp, typename _Ref, typename _Ptr>
    inline bool
    operator<(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	      const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    { return (__x._M_node == __y._M_node) ? (__x._M_cur < __y._M_cur)
					  : (__x._M_node < __y._M_node); }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline bool
    operator<(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	      const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    { return (__x._M_node == __y._M_node) ? (__x._M_cur < __y._M_cur)
					  : (__x._M_node < __y._M_node); }

  template<typename _Tp, typename _Ref, typename _Ptr>
    inline bool
    operator>(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	      const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    { return __y < __x; }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline bool
    operator>(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	      const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    { return __y < __x; }

  template<typename _Tp, typename _Ref, typename _Ptr>
    inline bool
    operator<=(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	       const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    { return !(__y < __x); }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline bool
    operator<=(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	       const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    { return !(__y < __x); }

  template<typename _Tp, typename _Ref, typename _Ptr>
    inline bool
    operator>=(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	       const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    { return !(__x < __y); }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline bool
    operator>=(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	       const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    { return !(__x < __y); }

  // _GLIBCXX_RESOLVE_LIB_DEFECTS
  // According to the resolution of DR179 not only the various comparison
  // operators but also operator- must accept mixed iterator/const_iterator
  // parameters.
  template<typename _Tp, typename _Ref, typename _Ptr>
    inline typename _Deque_iterator<_Tp, _Ref, _Ptr>::difference_type
    operator-(const _Deque_iterator<_Tp, _Ref, _Ptr>& __x,
	      const _Deque_iterator<_Tp, _Ref, _Ptr>& __y) _GLIBCXX_NOEXCEPT
    {
      return typename _Deque_iterator<_Tp, _Ref, _Ptr>::difference_type
	(_Deque_iterator<_Tp, _Ref, _Ptr>::_S_buffer_size())
	* (__x._M_node - __y._M_node - 1) + (__x._M_cur - __x._M_first)
	+ (__y._M_last - __y._M_cur);
    }

  template<typename _Tp, typename _RefL, typename _PtrL,
	   typename _RefR, typename _PtrR>
    inline typename _Deque_iterator<_Tp, _RefL, _PtrL>::difference_type
    operator-(const _Deque_iterator<_Tp, _RefL, _PtrL>& __x,
	      const _Deque_iterator<_Tp, _RefR, _PtrR>& __y) _GLIBCXX_NOEXCEPT
    {
      return typename _Deque_iterator<_Tp, _RefL, _PtrL>::difference_type
	(_Deque_iterator<_Tp, _RefL, _PtrL>::_S_buffer_size())
	* (__x._M_node - __y._M_node - 1) + (__x._M_cur - __x._M_first)
	+ (__y._M_last - __y._M_cur);
    }

  template<typename _Tp, typename _Ref, typename _Ptr>
    inline _Deque_iterator<_Tp, _Ref, _Ptr>
    operator+(ptrdiff_t __n, const _Deque_iterator<_Tp, _Ref, _Ptr>& __x)
    _GLIBCXX_NOEXCEPT
    { return __x + __n; }

  template<typename _Tp>
    void
    fill(const _Deque_iterator<_Tp, _Tp&, _Tp*>&,
	 const _Deque_iterator<_Tp, _Tp&, _Tp*>&, const _Tp&);

  template<typename _Tp>
    _Deque_iterator<_Tp, _Tp&, _Tp*>
    copy(_Deque_iterator<_Tp, const _Tp&, const _Tp*>,
	 _Deque_iterator<_Tp, const _Tp&, const _Tp*>,
	 _Deque_iterator<_Tp, _Tp&, _Tp*>);

  template<typename _Tp>
    inline _Deque_iterator<_Tp, _Tp&, _Tp*>
    copy(_Deque_iterator<_Tp, _Tp&, _Tp*> __first,
	 _Deque_iterator<_Tp, _Tp&, _Tp*> __last,
	 _Deque_iterator<_Tp, _Tp&, _Tp*> __result)
    { return std::copy(_Deque_iterator<_Tp, const _Tp&, const _Tp*>(__first),
		       _Deque_iterator<_Tp, const _Tp&, const _Tp*>(__last),
		       __result); }

  template<typename _Tp>
    _Deque_iterator<_Tp, _Tp&, _Tp*>
    copy_backward(_Deque_iterator<_Tp, const _Tp&, const _Tp*>,
		  _Deque_iterator<_Tp, const _Tp&, const _Tp*>,
		  _Deque_iterator<_Tp, _Tp&, _Tp*>);

  template<typename _Tp>
    inline _Deque_iterator<_Tp, _Tp&, _Tp*>
    copy_backward(_Deque_iterator<_Tp, _Tp&, _Tp*> __first,
		  _Deque_iterator<_Tp, _Tp&, _Tp*> __last,
		  _Deque_iterator<_Tp, _Tp&, _Tp*> __result)
    { return std::copy_backward(_Deque_iterator<_Tp,
				const _Tp&, const _Tp*>(__first),
				_Deque_iterator<_Tp,
				const _Tp&, const _Tp*>(__last),
				__result); }

#if __cplusplus >= 201103L
  template<typename _Tp>
    _Deque_iterator<_Tp, _Tp&, _Tp*>
    move(_Deque_iterator<_Tp, const _Tp&, const _Tp*>,
	 _Deque_iterator<_Tp, const _Tp&, const _Tp*>,
	 _Deque_iterator<_Tp, _Tp&, _Tp*>);

  template<typename _Tp>
    inline _Deque_iterator<_Tp, _Tp&, _Tp*>
    move(_Deque_iterator<_Tp, _Tp&, _Tp*> __first,
	 _Deque_iterator<_Tp, _Tp&, _Tp*> __last,
	 _Deque_iterator<_Tp, _Tp&, _Tp*> __result)
    { return std::move(_Deque_iterator<_Tp, const _Tp&, const _Tp*>(__first),
		       _Deque_iterator<_Tp, const _Tp&, const _Tp*>(__last),
		       __result); }

  template<typename _Tp>
    _Deque_iterator<_Tp, _Tp&, _Tp*>
    move_backward(_Deque_iterator<_Tp, const _Tp&, const _Tp*>,
		  _Deque_iterator<_Tp, const _Tp&, const _Tp*>,
		  _Deque_iterator<_Tp, _Tp&, _Tp*>);

  template<typename _Tp>
    inline _Deque_iterator<_Tp, _Tp&, _Tp*>
    move_backward(_Deque_iterator<_Tp, _Tp&, _Tp*> __first,
		  _Deque_iterator<_Tp, _Tp&, _Tp*> __last,
		  _Deque_iterator<_Tp, _Tp&, _Tp*> __result)
    { return std::move_backward(_Deque_iterator<_Tp,
				const _Tp&, const _Tp*>(__first),
				_Deque_iterator<_Tp,
				const _Tp&, const _Tp*>(__last),
				__result); }
#endif