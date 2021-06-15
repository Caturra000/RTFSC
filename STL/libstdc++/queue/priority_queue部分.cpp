// 一个独特的适配器（适配器说明见stack）
// 底层默认使用vector
// 真正构造comparable的流程是std::make_heap / std::push_heap / std::pop_heap
// 并且maintainer也在注释里直接说明了，只有插入删除等显式操作才能更改堆的结构，直接修改内部的值是无法做到重新构造成堆的
// （毕竟不是key-value形式，因此像top()这种接口提供的是const_reference，避免修改用于被_Compare调用的键值）

  /**
   *  @brief  A standard container automatically sorting its contents.
   *
   *  @ingroup sequences
   *
   *  @tparam _Tp  Type of element.
   *  @tparam _Sequence  Type of underlying sequence, defaults to vector<_Tp>.
   *  @tparam _Compare  Comparison function object type, defaults to
   *                    less<_Sequence::value_type>.
   *
   *  This is not a true container, but an @e adaptor.  It holds
   *  another container, and provides a wrapper interface to that
   *  container.  The wrapper is what enforces priority-based sorting
   *  and %queue behavior.  Very few of the standard container/sequence
   *  interface requirements are met (e.g., iterators).
   *
   *  The second template parameter defines the type of the underlying
   *  sequence/container.  It defaults to std::vector, but it can be
   *  any type that supports @c front(), @c push_back, @c pop_back,
   *  and random-access iterators, such as std::deque or an
   *  appropriate user-defined type.
   *
   *  The third template parameter supplies the means of making
   *  priority comparisons.  It defaults to @c less<value_type> but
   *  can be anything defining a strict weak ordering.
   *
   *  Members not found in @a normal containers are @c container_type,
   *  which is a typedef for the second Sequence parameter, and @c
   *  push, @c pop, and @c top, which are standard %queue operations.
   *
   *  @note No equality/comparison operators are provided for
   *  %priority_queue.
   *
   *  @note Sorting of the elements takes place as they are added to,
   *  and removed from, the %priority_queue using the
   *  %priority_queue's member functions.  If you access the elements
   *  by other means, and change their data such that the sorting
   *  order would be different, the %priority_queue will not re-sort
   *  the elements for you.  (How could it know to do so?)
  */
  template<typename _Tp, typename _Sequence = vector<_Tp>,
	   typename _Compare  = less<typename _Sequence::value_type> >
    class priority_queue
    {
#ifdef _GLIBCXX_CONCEPT_CHECKS
      // concept requirements
      typedef typename _Sequence::value_type _Sequence_value_type;
# if __cplusplus < 201103L
      __glibcxx_class_requires(_Tp, _SGIAssignableConcept)
# endif
      __glibcxx_class_requires(_Sequence, _SequenceConcept)
      __glibcxx_class_requires(_Sequence, _RandomAccessContainerConcept)
      __glibcxx_class_requires2(_Tp, _Sequence_value_type, _SameTypeConcept)
      __glibcxx_class_requires4(_Compare, bool, _Tp, _Tp,
				_BinaryFunctionConcept)
#endif

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
      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // DR 2684. priority_queue lacking comparator typedef
      typedef	       _Compare				value_compare;

    protected:
      //  See queue::c for notes on these names.
      _Sequence  c;
      _Compare   comp;

    public:
      /**
       *  @brief  Default constructor creates no elements.
       */
#if __cplusplus < 201103L
      explicit
      priority_queue(const _Compare& __x = _Compare(),
		     const _Sequence& __s = _Sequence())
      : c(__s), comp(__x)
      { std::make_heap(c.begin(), c.end(), comp); }
#else
      template<typename _Seq = _Sequence, typename _Requires = typename
	       enable_if<__and_<is_default_constructible<_Compare>,
				is_default_constructible<_Seq>>::value>::type>
	priority_queue()
	: c(), comp() { }

      explicit
      priority_queue(const _Compare& __x, const _Sequence& __s)
      : c(__s), comp(__x)
      { std::make_heap(c.begin(), c.end(), comp); }

      explicit
      priority_queue(const _Compare& __x, _Sequence&& __s = _Sequence())
      : c(std::move(__s)), comp(__x)
      { std::make_heap(c.begin(), c.end(), comp); }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	explicit
	priority_queue(const _Alloc& __a)
	: c(__a), comp() { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	priority_queue(const _Compare& __x, const _Alloc& __a)
	: c(__a), comp(__x) { }

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 2537. Constructors [...] taking allocators should call make_heap
      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	priority_queue(const _Compare& __x, const _Sequence& __c,
		       const _Alloc& __a)
	: c(__c, __a), comp(__x)
	{ std::make_heap(c.begin(), c.end(), comp); }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	priority_queue(const _Compare& __x, _Sequence&& __c, const _Alloc& __a)
	: c(std::move(__c), __a), comp(__x)
	{ std::make_heap(c.begin(), c.end(), comp); }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	priority_queue(const priority_queue& __q, const _Alloc& __a)
	: c(__q.c, __a), comp(__q.comp) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	priority_queue(priority_queue&& __q, const _Alloc& __a)
	: c(std::move(__q.c), __a), comp(std::move(__q.comp)) { }
#endif

      /**
       *  @brief  Builds a %queue from a range.
       *  @param  __first  An input iterator.
       *  @param  __last  An input iterator.
       *  @param  __x  A comparison functor describing a strict weak ordering.
       *  @param  __s  An initial sequence with which to start.
       *
       *  Begins by copying @a __s, inserting a copy of the elements
       *  from @a [first,last) into the copy of @a __s, then ordering
       *  the copy according to @a __x.
       *
       *  For more information on function objects, see the
       *  documentation on @link functors functor base
       *  classes@endlink.
       */
#if __cplusplus < 201103L
      template<typename _InputIterator>
	priority_queue(_InputIterator __first, _InputIterator __last,
		       const _Compare& __x = _Compare(),
		       const _Sequence& __s = _Sequence())
	: c(__s), comp(__x)
	{
	  __glibcxx_requires_valid_range(__first, __last);
	  c.insert(c.end(), __first, __last);
	  std::make_heap(c.begin(), c.end(), comp);
	}
#else
      template<typename _InputIterator>
	priority_queue(_InputIterator __first, _InputIterator __last,
		       const _Compare& __x,
		       const _Sequence& __s)
	: c(__s), comp(__x)
	{
	  __glibcxx_requires_valid_range(__first, __last);
	  c.insert(c.end(), __first, __last);
	  std::make_heap(c.begin(), c.end(), comp);
	}

      template<typename _InputIterator>
	priority_queue(_InputIterator __first, _InputIterator __last,
		       const _Compare& __x = _Compare(),
		       _Sequence&& __s = _Sequence())
	: c(std::move(__s)), comp(__x)
	{
	  __glibcxx_requires_valid_range(__first, __last);
	  c.insert(c.end(), __first, __last);
	  std::make_heap(c.begin(), c.end(), comp);
	}
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
       *  Returns a read-only (constant) reference to the data at the first
       *  element of the %queue.
       */
      const_reference
      top() const
      {
	__glibcxx_requires_nonempty();
	return c.front();
      }

      /**
       *  @brief  Add data to the %queue.
       *  @param  __x  Data to be added.
       *
       *  This is a typical %queue operation.
       *  The time complexity of the operation depends on the underlying
       *  sequence.
       */
      void
      push(const value_type& __x)
      {
	c.push_back(__x);
	std::push_heap(c.begin(), c.end(), comp);
      }

#if __cplusplus >= 201103L
      void
      push(value_type&& __x)
      {
	c.push_back(std::move(__x));
	std::push_heap(c.begin(), c.end(), comp);
      }

      template<typename... _Args>
	void
	emplace(_Args&&... __args)
	{
	  c.emplace_back(std::forward<_Args>(__args)...);
	  std::push_heap(c.begin(), c.end(), comp);
	}
#endif

      /**
       *  @brief  Removes first element.
       *
       *  This is a typical %queue operation.  It shrinks the %queue
       *  by one.  The time complexity of the operation depends on the
       *  underlying sequence.
       *
       *  Note that no data is returned, and if the first element's
       *  data is needed, it should be retrieved before pop() is
       *  called.
       */
      void
      pop()
      {
	__glibcxx_requires_nonempty();
	std::pop_heap(c.begin(), c.end(), comp);
	c.pop_back();
      }

#if __cplusplus >= 201103L
      void
      swap(priority_queue& __pq)
      noexcept(__and_<
#if __cplusplus > 201402L || !defined(__STRICT_ANSI__) // c++1z or gnu++11
		 __is_nothrow_swappable<_Sequence>,
#else
		 __is_nothrow_swappable<_Tp>,
#endif
		 __is_nothrow_swappable<_Compare>
	       >::value)
      {
	using std::swap;
	swap(c, __pq.c);
	swap(comp, __pq.comp);
      }
#endif // __cplusplus >= 201103L
    };

#if __cpp_deduction_guides >= 201606
  template<typename _Compare, typename _Container,
	   typename = _RequireNotAllocator<_Compare>,
	   typename = _RequireNotAllocator<_Container>>
    priority_queue(_Compare, _Container)
    -> priority_queue<typename _Container::value_type, _Container, _Compare>;

  template<typename _InputIterator, typename _ValT
	   = typename iterator_traits<_InputIterator>::value_type,
	   typename _Compare = less<_ValT>,
	   typename _Container = vector<_ValT>,
	   typename = _RequireInputIter<_InputIterator>,
	   typename = _RequireNotAllocator<_Compare>,
	   typename = _RequireNotAllocator<_Container>>
    priority_queue(_InputIterator, _InputIterator, _Compare = _Compare(),
		   _Container = _Container())
    -> priority_queue<_ValT, _Container, _Compare>;

  template<typename _Compare, typename _Container, typename _Allocator,
	   typename = _RequireNotAllocator<_Compare>,
	   typename = _RequireNotAllocator<_Container>,
	   typename = _RequireAllocator<_Allocator>>
    priority_queue(_Compare, _Container, _Allocator)
    -> priority_queue<typename _Container::value_type, _Container, _Compare>;
#endif

  // No equality/comparison operators are provided for priority_queue.

#if __cplusplus >= 201103L
  template<typename _Tp, typename _Sequence, typename _Compare>
    inline
#if __cplusplus > 201402L || !defined(__STRICT_ANSI__) // c++1z or gnu++11
    // Constrained free swap overload, see p0185r1
    typename enable_if<__and_<__is_swappable<_Sequence>,
			      __is_swappable<_Compare>>::value>::type
#else
    void
#endif
    swap(priority_queue<_Tp, _Sequence, _Compare>& __x,
	 priority_queue<_Tp, _Sequence, _Compare>& __y)
    noexcept(noexcept(__x.swap(__y)))
    { __x.swap(__y); }

  template<typename _Tp, typename _Sequence, typename _Compare,
	   typename _Alloc>
    struct uses_allocator<priority_queue<_Tp, _Sequence, _Compare>, _Alloc>
    : public uses_allocator<_Sequence, _Alloc>::type { };
#endif // __cplusplus >= 201103L