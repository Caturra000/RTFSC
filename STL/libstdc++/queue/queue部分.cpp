// 适配器见stack部分说明（基本一致）
// queue默认使用deque作为底层容器

  /**
   *  @brief  A standard container giving FIFO behavior.
   *
   *  @ingroup sequences
   *
   *  @tparam _Tp  Type of element.
   *  @tparam _Sequence  Type of underlying sequence, defaults to deque<_Tp>.
   *
   *  Meets many of the requirements of a
   *  <a href="tables.html#65">container</a>,
   *  but does not define anything to do with iterators.  Very few of the
   *  other standard container interfaces are defined.
   *
   *  This is not a true container, but an @e adaptor.  It holds another
   *  container, and provides a wrapper interface to that container.  The
   *  wrapper is what enforces strict first-in-first-out %queue behavior.
   *
   *  The second template parameter defines the type of the underlying
   *  sequence/container.  It defaults to std::deque, but it can be any type
   *  that supports @c front, @c back, @c push_back, and @c pop_front,
   *  such as std::list or an appropriate user-defined type.
   *
   *  Members not found in @a normal containers are @c container_type,
   *  which is a typedef for the second Sequence parameter, and @c push and
   *  @c pop, which are standard %queue/FIFO operations.
  */
  template<typename _Tp, typename _Sequence = deque<_Tp> >
    class queue
    {
#ifdef _GLIBCXX_CONCEPT_CHECKS
      // concept requirements
      typedef typename _Sequence::value_type _Sequence_value_type;
# if __cplusplus < 201103L
      __glibcxx_class_requires(_Tp, _SGIAssignableConcept)
# endif
      __glibcxx_class_requires(_Sequence, _FrontInsertionSequenceConcept)
      __glibcxx_class_requires(_Sequence, _BackInsertionSequenceConcept)
      __glibcxx_class_requires2(_Tp, _Sequence_value_type, _SameTypeConcept)
#endif

      template<typename _Tp1, typename _Seq1>
	friend bool
	operator==(const queue<_Tp1, _Seq1>&, const queue<_Tp1, _Seq1>&);

      template<typename _Tp1, typename _Seq1>
	friend bool
	operator<(const queue<_Tp1, _Seq1>&, const queue<_Tp1, _Seq1>&);

#if __cplusplus >= 201103L
      template<typename _Alloc>
	using _Uses = typename
	  enable_if<uses_allocator<_Sequence, _Alloc>::value>::type;

#if __cplusplus >= 201703L
      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 2566. Requirements on the first template parameter of container
      // adaptors
      static_assert(is_same<_Tp, typename _Sequence::value_type>::value,
	  "value_type must be the same as the underlying container");
#endif // C++17
#endif // C++11

    public:
      typedef typename	_Sequence::value_type		value_type;
      typedef typename	_Sequence::reference		reference;
      typedef typename	_Sequence::const_reference	const_reference;
      typedef typename	_Sequence::size_type		size_type;
      typedef		_Sequence			container_type;

    protected:
      /*  Maintainers wondering why this isn't uglified as per style
       *  guidelines should note that this name is specified in the standard,
       *  C++98 [23.2.3.1].
       *  (Why? Presumably for the same reason that it's protected instead
       *  of private: to allow derivation.  But none of the other
       *  containers allow for derivation.  Odd.)
       */
       ///  @c c is the underlying container.
      _Sequence c;

    public:
      /**
       *  @brief  Default constructor creates no elements.
       */
#if __cplusplus < 201103L
      explicit
      queue(const _Sequence& __c = _Sequence())
      : c(__c) { }
#else
      template<typename _Seq = _Sequence, typename _Requires = typename
	       enable_if<is_default_constructible<_Seq>::value>::type>
	queue()
	: c() { }

      explicit
      queue(const _Sequence& __c)
      : c(__c) { }

      explicit
      queue(_Sequence&& __c)
      : c(std::move(__c)) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	explicit
	queue(const _Alloc& __a)
	: c(__a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	queue(const _Sequence& __c, const _Alloc& __a)
	: c(__c, __a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	queue(_Sequence&& __c, const _Alloc& __a)
	: c(std::move(__c), __a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	queue(const queue& __q, const _Alloc& __a)
	: c(__q.c, __a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	queue(queue&& __q, const _Alloc& __a)
	: c(std::move(__q.c), __a) { }
#endif

      /**
       *  Returns true if the %queue is empty.
       */
      _GLIBCXX_NODISCARD bool
      empty() const
      { return c.empty(); }

      /**  Returns the number of elements in the %queue.  */
      size_type
      size() const
      { return c.size(); }

      /**
       *  Returns a read/write reference to the data at the first
       *  element of the %queue.
       */
      reference
      front()
      {
	__glibcxx_requires_nonempty();
	return c.front();
      }

      /**
       *  Returns a read-only (constant) reference to the data at the first
       *  element of the %queue.
       */
      const_reference
      front() const
      {
	__glibcxx_requires_nonempty();
	return c.front();
      }

      /**
       *  Returns a read/write reference to the data at the last
       *  element of the %queue.
       */
      reference
      back()
      {
	__glibcxx_requires_nonempty();
	return c.back();
      }

      /**
       *  Returns a read-only (constant) reference to the data at the last
       *  element of the %queue.
       */
      const_reference
      back() const
      {
	__glibcxx_requires_nonempty();
	return c.back();
      }

      /**
       *  @brief  Add data to the end of the %queue.
       *  @param  __x  Data to be added.
       *
       *  This is a typical %queue operation.  The function creates an
       *  element at the end of the %queue and assigns the given data
       *  to it.  The time complexity of the operation depends on the
       *  underlying sequence.
       */
      void
      push(const value_type& __x)
      { c.push_back(__x); }

#if __cplusplus >= 201103L
      void
      push(value_type&& __x)
      { c.push_back(std::move(__x)); }

#if __cplusplus > 201402L
      template<typename... _Args>
	decltype(auto)
	emplace(_Args&&... __args)
	{ return c.emplace_back(std::forward<_Args>(__args)...); }
#else
      template<typename... _Args>
	void
	emplace(_Args&&... __args)
	{ c.emplace_back(std::forward<_Args>(__args)...); }
#endif
#endif

      /**
       *  @brief  Removes first element.
       *
       *  This is a typical %queue operation.  It shrinks the %queue by one.
       *  The time complexity of the operation depends on the underlying
       *  sequence.
       *
       *  Note that no data is returned, and if the first element's
       *  data is needed, it should be retrieved before pop() is
       *  called.
       */
      void
      pop()
      {
	__glibcxx_requires_nonempty();
	c.pop_front();
      }

#if __cplusplus >= 201103L
      void
      swap(queue& __q)
#if __cplusplus > 201402L || !defined(__STRICT_ANSI__) // c++1z or gnu++11
      noexcept(__is_nothrow_swappable<_Sequence>::value)
#else
      noexcept(__is_nothrow_swappable<_Tp>::value)
#endif
      {
	using std::swap;
	swap(c, __q.c);
      }
#endif // __cplusplus >= 201103L
    };

#if __cpp_deduction_guides >= 201606
  template<typename _Container,
	   typename = _RequireNotAllocator<_Container>>
    queue(_Container) -> queue<typename _Container::value_type, _Container>;

  template<typename _Container, typename _Allocator,
	   typename = _RequireNotAllocator<_Container>,
	   typename = _RequireAllocator<_Allocator>>
    queue(_Container, _Allocator)
    -> queue<typename _Container::value_type, _Container>;
#endif

  /**
   *  @brief  Queue equality comparison.
   *  @param  __x  A %queue.
   *  @param  __y  A %queue of the same type as @a __x.
   *  @return  True iff the size and elements of the queues are equal.
   *
   *  This is an equivalence relation.  Complexity and semantics depend on the
   *  underlying sequence type, but the expected rules are:  this relation is
   *  linear in the size of the sequences, and queues are considered equivalent
   *  if their sequences compare equal.
  */
  template<typename _Tp, typename _Seq>
    inline bool
    operator==(const queue<_Tp, _Seq>& __x, const queue<_Tp, _Seq>& __y)
    { return __x.c == __y.c; }

  /**
   *  @brief  Queue ordering relation.
   *  @param  __x  A %queue.
   *  @param  __y  A %queue of the same type as @a x.
   *  @return  True iff @a __x is lexicographically less than @a __y.
   *
   *  This is an total ordering relation.  Complexity and semantics
   *  depend on the underlying sequence type, but the expected rules
   *  are: this relation is linear in the size of the sequences, the
   *  elements must be comparable with @c <, and
   *  std::lexicographical_compare() is usually used to make the
   *  determination.
  */
  template<typename _Tp, typename _Seq>
    inline bool
    operator<(const queue<_Tp, _Seq>& __x, const queue<_Tp, _Seq>& __y)
    { return __x.c < __y.c; }

  /// Based on operator==
  template<typename _Tp, typename _Seq>
    inline bool
    operator!=(const queue<_Tp, _Seq>& __x, const queue<_Tp, _Seq>& __y)
    { return !(__x == __y); }

  /// Based on operator<
  template<typename _Tp, typename _Seq>
    inline bool
    operator>(const queue<_Tp, _Seq>& __x, const queue<_Tp, _Seq>& __y)
    { return __y < __x; }

  /// Based on operator<
  template<typename _Tp, typename _Seq>
    inline bool
    operator<=(const queue<_Tp, _Seq>& __x, const queue<_Tp, _Seq>& __y)
    { return !(__y < __x); }

  /// Based on operator<
  template<typename _Tp, typename _Seq>
    inline bool
    operator>=(const queue<_Tp, _Seq>& __x, const queue<_Tp, _Seq>& __y)
    { return !(__x < __y); }

#if __cplusplus >= 201103L
  template<typename _Tp, typename _Seq>
    inline
#if __cplusplus > 201402L || !defined(__STRICT_ANSI__) // c++1z or gnu++11
    // Constrained free swap overload, see p0185r1
    typename enable_if<__is_swappable<_Seq>::value>::type
#else
    void
#endif
    swap(queue<_Tp, _Seq>& __x, queue<_Tp, _Seq>& __y)
    noexcept(noexcept(__x.swap(__y)))
    { __x.swap(__y); }

  template<typename _Tp, typename _Seq, typename _Alloc>
    struct uses_allocator<queue<_Tp, _Seq>, _Alloc>
    : public uses_allocator<_Seq, _Alloc>::type { };
#endif // __cplusplus >= 201103L