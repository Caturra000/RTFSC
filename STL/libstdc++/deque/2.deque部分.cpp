// deque继承_Deque_base
// 该文件主要是接口部分

// Question. deque的map为什么不用vector封装？
// Question. deque如何保证扩容也不会令node的引用失效？

  /**
   *  @brief  A standard container using fixed-size memory allocation and
   *  constant-time manipulation of elements at either end.
   *
   *  @ingroup sequences
   *
   *  @tparam _Tp  Type of element.
   *  @tparam _Alloc  Allocator type, defaults to allocator<_Tp>.
   *
   *  Meets the requirements of a <a href="tables.html#65">container</a>, a
   *  <a href="tables.html#66">reversible container</a>, and a
   *  <a href="tables.html#67">sequence</a>, including the
   *  <a href="tables.html#68">optional sequence requirements</a>.
   *
   *  In previous HP/SGI versions of deque, there was an extra template
   *  parameter so users could control the node size.  This extension turned
   *  out to violate the C++ standard (it can be detected using template
   *  template parameters), and it was removed.
   *
   *  Here's how a deque<Tp> manages memory.  Each deque has 4 members:
   *
   *  - Tp**        _M_map
   *  - size_t      _M_map_size
   *  - iterator    _M_start, _M_finish
   *
   *  map_size is at least 8.  %map is an array of map_size
   *  pointers-to-@a nodes.  (The name %map has nothing to do with the
   *  std::map class, and @b nodes should not be confused with
   *  std::list's usage of @a node.)
   *
   *  A @a node has no specific type name as such, but it is referred                 // 关键说明
   *  to as @a node in this file.  It is a simple array-of-Tp.  If Tp
   *  is very large, there will be one Tp element per node (i.e., an
   *  @a array of one).  For non-huge Tp's, node size is inversely
   *  related to Tp size: the larger the Tp, the fewer Tp's will fit
   *  in a node.  The goal here is to keep the total size of a node
   *  relatively small and constant over different Tp's, to improve
   *  allocator efficiency.
   *
   *  Not every pointer in the %map array will point to a node.  If
   *  the initial number of elements in the deque is small, the
   *  /middle/ %map pointers will be valid, and the ones at the edges
   *  will be unused.  This same situation will arise as the %map
   *  grows: available %map pointers, if any, will be on the ends.  As
   *  new nodes are created, only a subset of the %map's pointers need
   *  to be copied @a outward.
   *
   *  Class invariants:
   * - For any nonsingular iterator i:
   *    - i.node points to a member of the %map array.  (Yes, you read that
   *      correctly:  i.node does not actually point to a node.)  The member of
   *      the %map array is what actually points to the node.
   *    - i.first == *(i.node)    (This points to the node (first Tp element).)
   *    - i.last  == i.first + node_size
   *    - i.cur is a pointer in the range [i.first, i.last).  NOTE:
   *      the implication of this is that i.cur is always a dereferenceable
   *      pointer, even if i is a past-the-end iterator.
   * - Start and Finish are always nonsingular iterators.  NOTE: this
   * means that an empty deque must have one node, a deque with <N
   * elements (where N is the node buffer size) must have one node, a
   * deque with N through (2N-1) elements must have two nodes, etc.
   * - For every node other than start.node and finish.node, every
   * element in the node is an initialized object.  If start.node ==
   * finish.node, then [start.cur, finish.cur) are initialized
   * objects, and the elements outside that range are uninitialized
   * storage.  Otherwise, [start.cur, start.last) and [finish.first,
   * finish.cur) are initialized objects, and [start.first, start.cur)
   * and [finish.cur, finish.last) are uninitialized storage.
   * - [%map, %map + map_size) is a valid, non-empty range.
   * - [start.node, finish.node] is a valid range contained within
   *   [%map, %map + map_size).
   * - A pointer in the range [%map, %map + map_size) points to an allocated
   *   node if and only if the pointer is in the range
   *   [start.node, finish.node].
   *
   *  Here's the magic:  nothing in deque is @b aware of the discontiguous
   *  storage!
   *
   *  The memory setup and layout occurs in the parent, _Base, and the iterator
   *  class is entirely responsible for @a leaping from one node to the next.
   *  All the implementation routines for deque itself work only through the
   *  start and finish iterators.  This keeps the routines simple and sane,
   *  and we can use other standard algorithms as well.
  */
  template<typename _Tp, typename _Alloc = std::allocator<_Tp> >
    class deque : protected _Deque_base<_Tp, _Alloc>
    {
#ifdef _GLIBCXX_CONCEPT_CHECKS
      // concept requirements
      typedef typename _Alloc::value_type	_Alloc_value_type;
# if __cplusplus < 201103L
      __glibcxx_class_requires(_Tp, _SGIAssignableConcept)
# endif
      __glibcxx_class_requires2(_Tp, _Alloc_value_type, _SameTypeConcept)
#endif

#if __cplusplus >= 201103L
      static_assert(is_same<typename remove_cv<_Tp>::type, _Tp>::value,
	  "std::deque must have a non-const, non-volatile value_type");
# ifdef __STRICT_ANSI__
      static_assert(is_same<typename _Alloc::value_type, _Tp>::value,
	  "std::deque must have the same value_type as its allocator");
# endif
#endif

      typedef _Deque_base<_Tp, _Alloc>			_Base;
      typedef typename _Base::_Tp_alloc_type		_Tp_alloc_type;
      typedef typename _Base::_Alloc_traits		_Alloc_traits;
      typedef typename _Base::_Map_pointer		_Map_pointer;

    public:
      typedef _Tp					value_type;
      typedef typename _Alloc_traits::pointer		pointer;
      typedef typename _Alloc_traits::const_pointer	const_pointer;
      typedef typename _Alloc_traits::reference		reference;
      typedef typename _Alloc_traits::const_reference	const_reference;
      typedef typename _Base::iterator			iterator;
      typedef typename _Base::const_iterator		const_iterator;
      typedef std::reverse_iterator<const_iterator>	const_reverse_iterator;
      typedef std::reverse_iterator<iterator>		reverse_iterator;
      typedef size_t					size_type;
      typedef ptrdiff_t					difference_type;
      typedef _Alloc					allocator_type;

    protected:
      static size_t _S_buffer_size() _GLIBCXX_NOEXCEPT
      { return __deque_buf_size(sizeof(_Tp)); }

      // Functions controlling memory layout, and nothing else.
      using _Base::_M_initialize_map;
      using _Base::_M_create_nodes;
      using _Base::_M_destroy_nodes;
      using _Base::_M_allocate_node;
      using _Base::_M_deallocate_node;
      using _Base::_M_allocate_map;
      using _Base::_M_deallocate_map;
      using _Base::_M_get_Tp_allocator;

      /**
       *  A total of four data members accumulated down the hierarchy.
       *  May be accessed via _M_impl.*
       */
      using _Base::_M_impl;

    public:
      // [23.2.1.1] construct/copy/destroy
      // (assign() and get_allocator() are also listed in this section)

      /**
       *  @brief  Creates a %deque with no elements.
       */
      deque() : _Base() { }

      /**
       *  @brief  Creates a %deque with no elements.
       *  @param  __a  An allocator object.
       */
      explicit
      deque(const allocator_type& __a)
      : _Base(__a, 0) { }

#if __cplusplus >= 201103L
      /**
       *  @brief  Creates a %deque with default constructed elements.
       *  @param  __n  The number of elements to initially create.
       *  @param  __a  An allocator.
       *
       *  This constructor fills the %deque with @a n default
       *  constructed elements.
       */
      explicit
      deque(size_type __n, const allocator_type& __a = allocator_type())
      : _Base(__a, _S_check_init_len(__n, __a))
      { _M_default_initialize(); }

      /**
       *  @brief  Creates a %deque with copies of an exemplar element.
       *  @param  __n  The number of elements to initially create.
       *  @param  __value  An element to copy.
       *  @param  __a  An allocator.
       *
       *  This constructor fills the %deque with @a __n copies of @a __value.
       */
      deque(size_type __n, const value_type& __value,
	    const allocator_type& __a = allocator_type())
      : _Base(__a, _S_check_init_len(__n, __a))
      { _M_fill_initialize(__value); }
#else
      /**
       *  @brief  Creates a %deque with copies of an exemplar element.
       *  @param  __n  The number of elements to initially create.
       *  @param  __value  An element to copy.
       *  @param  __a  An allocator.
       *
       *  This constructor fills the %deque with @a __n copies of @a __value.
       */
      explicit
      deque(size_type __n, const value_type& __value = value_type(),
	    const allocator_type& __a = allocator_type())
      : _Base(__a, _S_check_init_len(__n, __a))
      { _M_fill_initialize(__value); }
#endif

      /**
       *  @brief  %Deque copy constructor.
       *  @param  __x  A %deque of identical element and allocator types.
       *
       *  The newly-created %deque uses a copy of the allocator object used
       *  by @a __x (unless the allocator traits dictate a different object).
       */
      deque(const deque& __x)
      : _Base(_Alloc_traits::_S_select_on_copy(__x._M_get_Tp_allocator()),
	      __x.size())
      { std::__uninitialized_copy_a(__x.begin(), __x.end(),
				    this->_M_impl._M_start,
				    _M_get_Tp_allocator()); }

#if __cplusplus >= 201103L
      /**
       *  @brief  %Deque move constructor.
       *  @param  __x  A %deque of identical element and allocator types.
       *
       *  The newly-created %deque contains the exact contents of @a __x.
       *  The contents of @a __x are a valid, but unspecified %deque.
       */
      deque(deque&& __x)
      : _Base(std::move(__x)) { }

      /// Copy constructor with alternative allocator
      deque(const deque& __x, const allocator_type& __a)
      : _Base(__a, __x.size())
      { std::__uninitialized_copy_a(__x.begin(), __x.end(),
				    this->_M_impl._M_start,
				    _M_get_Tp_allocator()); }

      /// Move constructor with alternative allocator
      deque(deque&& __x, const allocator_type& __a)
      : _Base(std::move(__x), __a, __x.size())
      {
	if (__x.get_allocator() != __a)
	  {
	    std::__uninitialized_move_a(__x.begin(), __x.end(),
					this->_M_impl._M_start,
					_M_get_Tp_allocator());
	    __x.clear();
	  }
      }

      /**
       *  @brief  Builds a %deque from an initializer list.
       *  @param  __l  An initializer_list.
       *  @param  __a  An allocator object.
       *
       *  Create a %deque consisting of copies of the elements in the
       *  initializer_list @a __l.
       *
       *  This will call the element type's copy constructor N times
       *  (where N is __l.size()) and do no memory reallocation.
       */
      deque(initializer_list<value_type> __l,
	    const allocator_type& __a = allocator_type())
      : _Base(__a)
      {
	_M_range_initialize(__l.begin(), __l.end(),
			    random_access_iterator_tag());
      }
#endif

      /**
       *  @brief  Builds a %deque from a range.
       *  @param  __first  An input iterator.
       *  @param  __last  An input iterator.
       *  @param  __a  An allocator object.
       *
       *  Create a %deque consisting of copies of the elements from [__first,
       *  __last).
       *
       *  If the iterators are forward, bidirectional, or random-access, then
       *  this will call the elements' copy constructor N times (where N is
       *  distance(__first,__last)) and do no memory reallocation.  But if only
       *  input iterators are used, then this will do at most 2N calls to the
       *  copy constructor, and logN memory reallocations.
       */
#if __cplusplus >= 201103L
      template<typename _InputIterator,
	       typename = std::_RequireInputIter<_InputIterator>>
	deque(_InputIterator __first, _InputIterator __last,
	      const allocator_type& __a = allocator_type())
	: _Base(__a)
	{ _M_initialize_dispatch(__first, __last, __false_type()); }
#else
      template<typename _InputIterator>
	deque(_InputIterator __first, _InputIterator __last,
	      const allocator_type& __a = allocator_type())
	: _Base(__a)
	{
	  // Check whether it's an integral type.  If so, it's not an iterator.
	  typedef typename std::__is_integer<_InputIterator>::__type _Integral;
	  _M_initialize_dispatch(__first, __last, _Integral());
	}
#endif

      /**
       *  The dtor only erases the elements, and note that if the elements
       *  themselves are pointers, the pointed-to memory is not touched in any
       *  way.  Managing the pointer is the user's responsibility.
       */
      ~deque()
      { _M_destroy_data(begin(), end(), _M_get_Tp_allocator()); }

      /**
       *  @brief  %Deque assignment operator.
       *  @param  __x  A %deque of identical element and allocator types.
       *
       *  All the elements of @a x are copied.
       *
       *  The newly-created %deque uses a copy of the allocator object used
       *  by @a __x (unless the allocator traits dictate a different object).
       */
      deque&
      operator=(const deque& __x);

#if __cplusplus >= 201103L
      /**
       *  @brief  %Deque move assignment operator.
       *  @param  __x  A %deque of identical element and allocator types.
       *
       *  The contents of @a __x are moved into this deque (without copying,
       *  if the allocators permit it).
       *  @a __x is a valid, but unspecified %deque.
       */
      deque&
      operator=(deque&& __x) noexcept(_Alloc_traits::_S_always_equal())
      {
	using __always_equal = typename _Alloc_traits::is_always_equal;
	_M_move_assign1(std::move(__x), __always_equal{});
	return *this;
      }

      /**
       *  @brief  Assigns an initializer list to a %deque.
       *  @param  __l  An initializer_list.
       *
       *  This function fills a %deque with copies of the elements in the
       *  initializer_list @a __l.
       *
       *  Note that the assignment completely changes the %deque and that the
       *  resulting %deque's size is the same as the number of elements
       *  assigned.
       */
      deque&
      operator=(initializer_list<value_type> __l)
      {
	_M_assign_aux(__l.begin(), __l.end(),
		      random_access_iterator_tag());
	return *this;
      }
#endif

      /**
       *  @brief  Assigns a given value to a %deque.
       *  @param  __n  Number of elements to be assigned.
       *  @param  __val  Value to be assigned.
       *
       *  This function fills a %deque with @a n copies of the given
       *  value.  Note that the assignment completely changes the
       *  %deque and that the resulting %deque's size is the same as
       *  the number of elements assigned.
       */
      void
      assign(size_type __n, const value_type& __val)
      { _M_fill_assign(__n, __val); }

      /**
       *  @brief  Assigns a range to a %deque.
       *  @param  __first  An input iterator.
       *  @param  __last   An input iterator.
       *
       *  This function fills a %deque with copies of the elements in the
       *  range [__first,__last).
       *
       *  Note that the assignment completely changes the %deque and that the
       *  resulting %deque's size is the same as the number of elements
       *  assigned.
       */
#if __cplusplus >= 201103L
      template<typename _InputIterator,
	       typename = std::_RequireInputIter<_InputIterator>>
	void
	assign(_InputIterator __first, _InputIterator __last)
	{ _M_assign_dispatch(__first, __last, __false_type()); }
#else
      template<typename _InputIterator>
	void
	assign(_InputIterator __first, _InputIterator __last)
	{
	  typedef typename std::__is_integer<_InputIterator>::__type _Integral;
	  _M_assign_dispatch(__first, __last, _Integral());
	}
#endif

#if __cplusplus >= 201103L
      /**
       *  @brief  Assigns an initializer list to a %deque.
       *  @param  __l  An initializer_list.
       *
       *  This function fills a %deque with copies of the elements in the
       *  initializer_list @a __l.
       *
       *  Note that the assignment completely changes the %deque and that the
       *  resulting %deque's size is the same as the number of elements
       *  assigned.
       */
      void
      assign(initializer_list<value_type> __l)
      { _M_assign_aux(__l.begin(), __l.end(), random_access_iterator_tag()); }
#endif

      /// Get a copy of the memory allocation object.
      allocator_type
      get_allocator() const _GLIBCXX_NOEXCEPT
      { return _Base::get_allocator(); }

      // iterators
      /**
       *  Returns a read/write iterator that points to the first element in the
       *  %deque.  Iteration is done in ordinary element order.
       */
      iterator
      begin() _GLIBCXX_NOEXCEPT
      { return this->_M_impl._M_start; }

      /**
       *  Returns a read-only (constant) iterator that points to the first
       *  element in the %deque.  Iteration is done in ordinary element order.
       */
      const_iterator
      begin() const _GLIBCXX_NOEXCEPT
      { return this->_M_impl._M_start; }

      /**
       *  Returns a read/write iterator that points one past the last
       *  element in the %deque.  Iteration is done in ordinary
       *  element order.
       */
      iterator
      end() _GLIBCXX_NOEXCEPT
      { return this->_M_impl._M_finish; }

      /**
       *  Returns a read-only (constant) iterator that points one past
       *  the last element in the %deque.  Iteration is done in
       *  ordinary element order.
       */
      const_iterator
      end() const _GLIBCXX_NOEXCEPT
      { return this->_M_impl._M_finish; }

      /**
       *  Returns a read/write reverse iterator that points to the
       *  last element in the %deque.  Iteration is done in reverse
       *  element order.
       */
      reverse_iterator
      rbegin() _GLIBCXX_NOEXCEPT
      { return reverse_iterator(this->_M_impl._M_finish); }

      /**
       *  Returns a read-only (constant) reverse iterator that points
       *  to the last element in the %deque.  Iteration is done in
       *  reverse element order.
       */
      const_reverse_iterator
      rbegin() const _GLIBCXX_NOEXCEPT
      { return const_reverse_iterator(this->_M_impl._M_finish); }

      /**
       *  Returns a read/write reverse iterator that points to one
       *  before the first element in the %deque.  Iteration is done
       *  in reverse element order.
       */
      reverse_iterator
      rend() _GLIBCXX_NOEXCEPT
      { return reverse_iterator(this->_M_impl._M_start); }

      /**
       *  Returns a read-only (constant) reverse iterator that points
       *  to one before the first element in the %deque.  Iteration is
       *  done in reverse element order.
       */
      const_reverse_iterator
      rend() const _GLIBCXX_NOEXCEPT
      { return const_reverse_iterator(this->_M_impl._M_start); }

#if __cplusplus >= 201103L
      /**
       *  Returns a read-only (constant) iterator that points to the first
       *  element in the %deque.  Iteration is done in ordinary element order.
       */
      const_iterator
      cbegin() const noexcept
      { return this->_M_impl._M_start; }

      /**
       *  Returns a read-only (constant) iterator that points one past
       *  the last element in the %deque.  Iteration is done in
       *  ordinary element order.
       */
      const_iterator
      cend() const noexcept
      { return this->_M_impl._M_finish; }

      /**
       *  Returns a read-only (constant) reverse iterator that points
       *  to the last element in the %deque.  Iteration is done in
       *  reverse element order.
       */
      const_reverse_iterator
      crbegin() const noexcept
      { return const_reverse_iterator(this->_M_impl._M_finish); }

      /**
       *  Returns a read-only (constant) reverse iterator that points
       *  to one before the first element in the %deque.  Iteration is
       *  done in reverse element order.
       */
      const_reverse_iterator
      crend() const noexcept
      { return const_reverse_iterator(this->_M_impl._M_start); }
#endif

      // [23.2.1.2] capacity
      /**  Returns the number of elements in the %deque.  */
      size_type
      size() const _GLIBCXX_NOEXCEPT
      { return this->_M_impl._M_finish - this->_M_impl._M_start; }

      /**  Returns the size() of the largest possible %deque.  */
      size_type
      max_size() const _GLIBCXX_NOEXCEPT
      { return _S_max_size(_M_get_Tp_allocator()); }

#if __cplusplus >= 201103L
      /**
       *  @brief  Resizes the %deque to the specified number of elements.
       *  @param  __new_size  Number of elements the %deque should contain.
       *
       *  This function will %resize the %deque to the specified
       *  number of elements.  If the number is smaller than the
       *  %deque's current size the %deque is truncated, otherwise
       *  default constructed elements are appended.
       */
      void
      resize(size_type __new_size)
      {
	const size_type __len = size();
	if (__new_size > __len)
	  _M_default_append(__new_size - __len);
	else if (__new_size < __len)
	  _M_erase_at_end(this->_M_impl._M_start
			  + difference_type(__new_size));
      }

      /**
       *  @brief  Resizes the %deque to the specified number of elements.
       *  @param  __new_size  Number of elements the %deque should contain.
       *  @param  __x  Data with which new elements should be populated.
       *
       *  This function will %resize the %deque to the specified
       *  number of elements.  If the number is smaller than the
       *  %deque's current size the %deque is truncated, otherwise the
       *  %deque is extended and new elements are populated with given
       *  data.
       */
      void
      resize(size_type __new_size, const value_type& __x)
      {
	const size_type __len = size();
	if (__new_size > __len)
	  _M_fill_insert(this->_M_impl._M_finish, __new_size - __len, __x);
	else if (__new_size < __len)
	  _M_erase_at_end(this->_M_impl._M_start
			  + difference_type(__new_size));
      }
#else
      /**
       *  @brief  Resizes the %deque to the specified number of elements.
       *  @param  __new_size  Number of elements the %deque should contain.
       *  @param  __x  Data with which new elements should be populated.
       *
       *  This function will %resize the %deque to the specified
       *  number of elements.  If the number is smaller than the
       *  %deque's current size the %deque is truncated, otherwise the
       *  %deque is extended and new elements are populated with given
       *  data.
       */
      void
      resize(size_type __new_size, value_type __x = value_type())
      {
	const size_type __len = size();
	if (__new_size > __len)
	  _M_fill_insert(this->_M_impl._M_finish, __new_size - __len, __x);
	else if (__new_size < __len)
	  _M_erase_at_end(this->_M_impl._M_start
			  + difference_type(__new_size));
      }
#endif

#if __cplusplus >= 201103L
      /**  A non-binding request to reduce memory use.  */
      void
      shrink_to_fit() noexcept
      { _M_shrink_to_fit(); }
#endif

      /**
       *  Returns true if the %deque is empty.  (Thus begin() would
       *  equal end().)
       */
      _GLIBCXX_NODISCARD bool
      empty() const _GLIBCXX_NOEXCEPT
      { return this->_M_impl._M_finish == this->_M_impl._M_start; }

      // element access
      /**
       *  @brief Subscript access to the data contained in the %deque.
       *  @param __n The index of the element for which data should be
       *  accessed.
       *  @return  Read/write reference to data.
       *
       *  This operator allows for easy, array-style, data access.
       *  Note that data access with this operator is unchecked and
       *  out_of_range lookups are not defined. (For checked lookups
       *  see at().)
       */
      reference
      operator[](size_type __n) _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_subscript(__n);
	return this->_M_impl._M_start[difference_type(__n)];
      }

      /**
       *  @brief Subscript access to the data contained in the %deque.
       *  @param __n The index of the element for which data should be
       *  accessed.
       *  @return  Read-only (constant) reference to data.
       *
       *  This operator allows for easy, array-style, data access.
       *  Note that data access with this operator is unchecked and
       *  out_of_range lookups are not defined. (For checked lookups
       *  see at().)
       */
      const_reference
      operator[](size_type __n) const _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_subscript(__n);
	return this->_M_impl._M_start[difference_type(__n)];
      }

    protected:
      /// Safety check used only from at().
      void
      _M_range_check(size_type __n) const
      {
	if (__n >= this->size())
	  __throw_out_of_range_fmt(__N("deque::_M_range_check: __n "
				       "(which is %zu)>= this->size() "
				       "(which is %zu)"),
				   __n, this->size());
      }

    public:
      /**
       *  @brief  Provides access to the data contained in the %deque.
       *  @param __n The index of the element for which data should be
       *  accessed.
       *  @return  Read/write reference to data.
       *  @throw  std::out_of_range  If @a __n is an invalid index.
       *
       *  This function provides for safer data access.  The parameter
       *  is first checked that it is in the range of the deque.  The
       *  function throws out_of_range if the check fails.
       */
      reference
      at(size_type __n)
      {
	_M_range_check(__n);
	return (*this)[__n];
      }

      /**
       *  @brief  Provides access to the data contained in the %deque.
       *  @param __n The index of the element for which data should be
       *  accessed.
       *  @return  Read-only (constant) reference to data.
       *  @throw  std::out_of_range  If @a __n is an invalid index.
       *
       *  This function provides for safer data access.  The parameter is first
       *  checked that it is in the range of the deque.  The function throws
       *  out_of_range if the check fails.
       */
      const_reference
      at(size_type __n) const
      {
	_M_range_check(__n);
	return (*this)[__n];
      }

      /**
       *  Returns a read/write reference to the data at the first
       *  element of the %deque.
       */
      reference
      front() _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_nonempty();
	return *begin();
      }

      /**
       *  Returns a read-only (constant) reference to the data at the first
       *  element of the %deque.
       */
      const_reference
      front() const _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_nonempty();
	return *begin();
      }

      /**
       *  Returns a read/write reference to the data at the last element of the
       *  %deque.
       */
      reference
      back() _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_nonempty();
	iterator __tmp = end();
	--__tmp;
	return *__tmp;
      }

      /**
       *  Returns a read-only (constant) reference to the data at the last
       *  element of the %deque.
       */
      const_reference
      back() const _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_nonempty();
	const_iterator __tmp = end();
	--__tmp;
	return *__tmp;
      }

      // [23.2.1.2] modifiers
      /**
       *  @brief  Add data to the front of the %deque.
       *  @param  __x  Data to be added.
       *
       *  This is a typical stack operation.  The function creates an
       *  element at the front of the %deque and assigns the given
       *  data to it.  Due to the nature of a %deque this operation
       *  can be done in constant time.
       */
      void
      push_front(const value_type& __x)
      {
	if (this->_M_impl._M_start._M_cur != this->_M_impl._M_start._M_first)
	  {
	    _Alloc_traits::construct(this->_M_impl,
				     this->_M_impl._M_start._M_cur - 1,
				     __x);
	    --this->_M_impl._M_start._M_cur;
	  }
	else
	  _M_push_front_aux(__x);
      }

#if __cplusplus >= 201103L
      void
      push_front(value_type&& __x)
      { emplace_front(std::move(__x)); }

      template<typename... _Args>
#if __cplusplus > 201402L
	reference
#else
	void
#endif
	emplace_front(_Args&&... __args);
#endif

      /**
       *  @brief  Add data to the end of the %deque.
       *  @param  __x  Data to be added.
       *
       *  This is a typical stack operation.  The function creates an
       *  element at the end of the %deque and assigns the given data
       *  to it.  Due to the nature of a %deque this operation can be
       *  done in constant time.
       */
      void
      push_back(const value_type& __x)
      {
	if (this->_M_impl._M_finish._M_cur
	    != this->_M_impl._M_finish._M_last - 1)
	  {
	    _Alloc_traits::construct(this->_M_impl,
				     this->_M_impl._M_finish._M_cur, __x);
	    ++this->_M_impl._M_finish._M_cur;
	  }
	else
	  _M_push_back_aux(__x);
      }

#if __cplusplus >= 201103L
      void
      push_back(value_type&& __x)
      { emplace_back(std::move(__x)); }

      template<typename... _Args>
#if __cplusplus > 201402L
	reference
#else
	void
#endif
	emplace_back(_Args&&... __args);
#endif

      /**
       *  @brief  Removes first element.
       *
       *  This is a typical stack operation.  It shrinks the %deque by one.
       *
       *  Note that no data is returned, and if the first element's data is
       *  needed, it should be retrieved before pop_front() is called.
       */
      void
      pop_front() _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_nonempty();
	if (this->_M_impl._M_start._M_cur
	    != this->_M_impl._M_start._M_last - 1)
	  {
	    _Alloc_traits::destroy(this->_M_impl,
				   this->_M_impl._M_start._M_cur);
	    ++this->_M_impl._M_start._M_cur;
	  }
	else
	  _M_pop_front_aux();
      }

      /**
       *  @brief  Removes last element.
       *
       *  This is a typical stack operation.  It shrinks the %deque by one.
       *
       *  Note that no data is returned, and if the last element's data is
       *  needed, it should be retrieved before pop_back() is called.
       */
      void
      pop_back() _GLIBCXX_NOEXCEPT
      {
	__glibcxx_requires_nonempty();
	if (this->_M_impl._M_finish._M_cur
	    != this->_M_impl._M_finish._M_first)
	  {
	    --this->_M_impl._M_finish._M_cur;
	    _Alloc_traits::destroy(this->_M_impl,
				   this->_M_impl._M_finish._M_cur);
	  }
	else
	  _M_pop_back_aux();
      }

#if __cplusplus >= 201103L
      /**
       *  @brief  Inserts an object in %deque before specified iterator.
       *  @param  __position  A const_iterator into the %deque.
       *  @param  __args  Arguments.
       *  @return  An iterator that points to the inserted data.
       *
       *  This function will insert an object of type T constructed
       *  with T(std::forward<Args>(args)...) before the specified location.
       */
      template<typename... _Args>
	iterator
	emplace(const_iterator __position, _Args&&... __args);

      /**
       *  @brief  Inserts given value into %deque before specified iterator.
       *  @param  __position  A const_iterator into the %deque.
       *  @param  __x  Data to be inserted.
       *  @return  An iterator that points to the inserted data.
       *
       *  This function will insert a copy of the given value before the
       *  specified location.
       */
      iterator
      insert(const_iterator __position, const value_type& __x);
#else
      /**
       *  @brief  Inserts given value into %deque before specified iterator.
       *  @param  __position  An iterator into the %deque.
       *  @param  __x  Data to be inserted.
       *  @return  An iterator that points to the inserted data.
       *
       *  This function will insert a copy of the given value before the
       *  specified location.
       */
      iterator
      insert(iterator __position, const value_type& __x);
#endif

#if __cplusplus >= 201103L
      /**
       *  @brief  Inserts given rvalue into %deque before specified iterator.
       *  @param  __position  A const_iterator into the %deque.
       *  @param  __x  Data to be inserted.
       *  @return  An iterator that points to the inserted data.
       *
       *  This function will insert a copy of the given rvalue before the
       *  specified location.
       */
      iterator
      insert(const_iterator __position, value_type&& __x)
      { return emplace(__position, std::move(__x)); }

      /**
       *  @brief  Inserts an initializer list into the %deque.
       *  @param  __p  An iterator into the %deque.
       *  @param  __l  An initializer_list.
       *
       *  This function will insert copies of the data in the
       *  initializer_list @a __l into the %deque before the location
       *  specified by @a __p.  This is known as <em>list insert</em>.
       */
      iterator
      insert(const_iterator __p, initializer_list<value_type> __l)
      {
	auto __offset = __p - cbegin();
	_M_range_insert_aux(__p._M_const_cast(), __l.begin(), __l.end(),
			    std::random_access_iterator_tag());
	return begin() + __offset;
      }
#endif

#if __cplusplus >= 201103L
      /**
       *  @brief  Inserts a number of copies of given data into the %deque.
       *  @param  __position  A const_iterator into the %deque.
       *  @param  __n  Number of elements to be inserted.
       *  @param  __x  Data to be inserted.
       *  @return  An iterator that points to the inserted data.
       *
       *  This function will insert a specified number of copies of the given
       *  data before the location specified by @a __position.
       */
      iterator
      insert(const_iterator __position, size_type __n, const value_type& __x)
      {
	difference_type __offset = __position - cbegin();
	_M_fill_insert(__position._M_const_cast(), __n, __x);
	return begin() + __offset;
      }
#else
      /**
       *  @brief  Inserts a number of copies of given data into the %deque.
       *  @param  __position  An iterator into the %deque.
       *  @param  __n  Number of elements to be inserted.
       *  @param  __x  Data to be inserted.
       *
       *  This function will insert a specified number of copies of the given
       *  data before the location specified by @a __position.
       */
      void
      insert(iterator __position, size_type __n, const value_type& __x)
      { _M_fill_insert(__position, __n, __x); }
#endif

#if __cplusplus >= 201103L
      /**
       *  @brief  Inserts a range into the %deque.
       *  @param  __position  A const_iterator into the %deque.
       *  @param  __first  An input iterator.
       *  @param  __last   An input iterator.
       *  @return  An iterator that points to the inserted data.
       *
       *  This function will insert copies of the data in the range
       *  [__first,__last) into the %deque before the location specified
       *  by @a __position.  This is known as <em>range insert</em>.
       */
      template<typename _InputIterator,
	       typename = std::_RequireInputIter<_InputIterator>>
	iterator
	insert(const_iterator __position, _InputIterator __first,
	       _InputIterator __last)
	{
	  difference_type __offset = __position - cbegin();
	  _M_insert_dispatch(__position._M_const_cast(),
			     __first, __last, __false_type());
	  return begin() + __offset;
	}
#else
      /**
       *  @brief  Inserts a range into the %deque.
       *  @param  __position  An iterator into the %deque.
       *  @param  __first  An input iterator.
       *  @param  __last   An input iterator.
       *
       *  This function will insert copies of the data in the range
       *  [__first,__last) into the %deque before the location specified
       *  by @a __position.  This is known as <em>range insert</em>.
       */
      template<typename _InputIterator>
	void
	insert(iterator __position, _InputIterator __first,
	       _InputIterator __last)
	{
	  // Check whether it's an integral type.  If so, it's not an iterator.
	  typedef typename std::__is_integer<_InputIterator>::__type _Integral;
	  _M_insert_dispatch(__position, __first, __last, _Integral());
	}
#endif

      /**
       *  @brief  Remove element at given position.
       *  @param  __position  Iterator pointing to element to be erased.
       *  @return  An iterator pointing to the next element (or end()).
       *
       *  This function will erase the element at the given position and thus
       *  shorten the %deque by one.
       *
       *  The user is cautioned that
       *  this function only erases the element, and that if the element is
       *  itself a pointer, the pointed-to memory is not touched in any way.
       *  Managing the pointer is the user's responsibility.
       */
      iterator
#if __cplusplus >= 201103L
      erase(const_iterator __position)
#else
      erase(iterator __position)
#endif
      { return _M_erase(__position._M_const_cast()); }

      /**
       *  @brief  Remove a range of elements.
       *  @param  __first  Iterator pointing to the first element to be erased.
       *  @param  __last  Iterator pointing to one past the last element to be
       *                erased.
       *  @return  An iterator pointing to the element pointed to by @a last
       *           prior to erasing (or end()).
       *
       *  This function will erase the elements in the range
       *  [__first,__last) and shorten the %deque accordingly.
       *
       *  The user is cautioned that
       *  this function only erases the elements, and that if the elements
       *  themselves are pointers, the pointed-to memory is not touched in any
       *  way.  Managing the pointer is the user's responsibility.
       */
      iterator
#if __cplusplus >= 201103L
      erase(const_iterator __first, const_iterator __last)
#else
      erase(iterator __first, iterator __last)
#endif
      { return _M_erase(__first._M_const_cast(), __last._M_const_cast()); }

      /**
       *  @brief  Swaps data with another %deque.
       *  @param  __x  A %deque of the same element and allocator types.
       *
       *  This exchanges the elements between two deques in constant time.
       *  (Four pointers, so it should be quite fast.)
       *  Note that the global std::swap() function is specialized such that
       *  std::swap(d1,d2) will feed to this function.
       *
       *  Whether the allocators are swapped depends on the allocator traits.
       */
      void
      swap(deque& __x) _GLIBCXX_NOEXCEPT
      {
#if __cplusplus >= 201103L
	__glibcxx_assert(_Alloc_traits::propagate_on_container_swap::value
			 || _M_get_Tp_allocator() == __x._M_get_Tp_allocator());
#endif
	_M_impl._M_swap_data(__x._M_impl);
	_Alloc_traits::_S_on_swap(_M_get_Tp_allocator(),
				  __x._M_get_Tp_allocator());
      }

      /**
       *  Erases all the elements.  Note that this function only erases the
       *  elements, and that if the elements themselves are pointers, the
       *  pointed-to memory is not touched in any way.  Managing the pointer is
       *  the user's responsibility.
       */
      void
      clear() _GLIBCXX_NOEXCEPT
      { _M_erase_at_end(begin()); }

    protected:
      // Internal constructor functions follow.

      // called by the range constructor to implement [23.1.1]/9

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 438. Ambiguity in the "do the right thing" clause
      template<typename _Integer>
	void
	_M_initialize_dispatch(_Integer __n, _Integer __x, __true_type)
	{
	  _M_initialize_map(_S_check_init_len(static_cast<size_type>(__n),
					      _M_get_Tp_allocator()));
	  _M_fill_initialize(__x);
	}

      static size_t
      _S_check_init_len(size_t __n, const allocator_type& __a)
      {
	if (__n > _S_max_size(__a))
	  __throw_length_error(
	      __N("cannot create std::deque larger than max_size()"));
	return __n;
      }

      static size_type
      _S_max_size(const _Tp_alloc_type& __a) _GLIBCXX_NOEXCEPT
      {
	const size_t __diffmax = __gnu_cxx::__numeric_traits<ptrdiff_t>::__max;
	const size_t __allocmax = _Alloc_traits::max_size(__a);
	return (std::min)(__diffmax, __allocmax);
      }

      // called by the range constructor to implement [23.1.1]/9
      template<typename _InputIterator>
	void
	_M_initialize_dispatch(_InputIterator __first, _InputIterator __last,
			       __false_type)
	{
	  _M_range_initialize(__first, __last,
			      std::__iterator_category(__first));
	}

      // called by the second initialize_dispatch above
      //@{
      /**
       *  @brief Fills the deque with whatever is in [first,last).
       *  @param  __first  An input iterator.
       *  @param  __last  An input iterator.
       *  @return   Nothing.
       *
       *  If the iterators are actually forward iterators (or better), then the
       *  memory layout can be done all at once.  Else we move forward using
       *  push_back on each value from the iterator.
       */
      template<typename _InputIterator>
	void
	_M_range_initialize(_InputIterator __first, _InputIterator __last,
			    std::input_iterator_tag);

      // called by the second initialize_dispatch above
      template<typename _ForwardIterator>
	void
	_M_range_initialize(_ForwardIterator __first, _ForwardIterator __last,
			    std::forward_iterator_tag);
      //@}

      /**
       *  @brief Fills the %deque with copies of value.
       *  @param  __value  Initial value.
       *  @return   Nothing.
       *  @pre _M_start and _M_finish have already been initialized,
       *  but none of the %deque's elements have yet been constructed.
       *
       *  This function is called only when the user provides an explicit size
       *  (with or without an explicit exemplar value).
       */
      void
      _M_fill_initialize(const value_type& __value);

#if __cplusplus >= 201103L
      // called by deque(n).
      void
      _M_default_initialize();
#endif

      // Internal assign functions follow.  The *_aux functions do the actual
      // assignment work for the range versions.

      // called by the range assign to implement [23.1.1]/9

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 438. Ambiguity in the "do the right thing" clause
      template<typename _Integer>
	void
	_M_assign_dispatch(_Integer __n, _Integer __val, __true_type)
	{ _M_fill_assign(__n, __val); }

      // called by the range assign to implement [23.1.1]/9
      template<typename _InputIterator>
	void
	_M_assign_dispatch(_InputIterator __first, _InputIterator __last,
			   __false_type)
	{ _M_assign_aux(__first, __last, std::__iterator_category(__first)); }

      // called by the second assign_dispatch above
      template<typename _InputIterator>
	void
	_M_assign_aux(_InputIterator __first, _InputIterator __last,
		      std::input_iterator_tag);

      // called by the second assign_dispatch above
      template<typename _ForwardIterator>
	void
	_M_assign_aux(_ForwardIterator __first, _ForwardIterator __last,
		      std::forward_iterator_tag)
	{
	  const size_type __len = std::distance(__first, __last);
	  if (__len > size())
	    {
	      _ForwardIterator __mid = __first;
	      std::advance(__mid, size());
	      std::copy(__first, __mid, begin());
	      _M_range_insert_aux(end(), __mid, __last,
				  std::__iterator_category(__first));
	    }
	  else
	    _M_erase_at_end(std::copy(__first, __last, begin()));
	}

      // Called by assign(n,t), and the range assign when it turns out
      // to be the same thing.
      void
      _M_fill_assign(size_type __n, const value_type& __val)
      {
	if (__n > size())
	  {
	    std::fill(begin(), end(), __val);
	    _M_fill_insert(end(), __n - size(), __val);
	  }
	else
	  {
	    _M_erase_at_end(begin() + difference_type(__n));
	    std::fill(begin(), end(), __val);
	  }
      }

      //@{
      /// Helper functions for push_* and pop_*.
#if __cplusplus < 201103L
      void _M_push_back_aux(const value_type&);

      void _M_push_front_aux(const value_type&);
#else
      template<typename... _Args>
	void _M_push_back_aux(_Args&&... __args);  // 见deque.tcc

      template<typename... _Args>
	void _M_push_front_aux(_Args&&... __args);
#endif

      void _M_pop_back_aux();

      void _M_pop_front_aux();
      //@}

      // Internal insert functions follow.  The *_aux functions do the actual
      // insertion work when all shortcuts fail.

      // called by the range insert to implement [23.1.1]/9

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 438. Ambiguity in the "do the right thing" clause
      template<typename _Integer>
	void
	_M_insert_dispatch(iterator __pos,
			   _Integer __n, _Integer __x, __true_type)
	{ _M_fill_insert(__pos, __n, __x); }

      // called by the range insert to implement [23.1.1]/9
      template<typename _InputIterator>
	void
	_M_insert_dispatch(iterator __pos,
			   _InputIterator __first, _InputIterator __last,
			   __false_type)
	{
	  _M_range_insert_aux(__pos, __first, __last,
			      std::__iterator_category(__first));
	}

      // called by the second insert_dispatch above
      template<typename _InputIterator>
	void
	_M_range_insert_aux(iterator __pos, _InputIterator __first,
			    _InputIterator __last, std::input_iterator_tag);

      // called by the second insert_dispatch above
      template<typename _ForwardIterator>
	void
	_M_range_insert_aux(iterator __pos, _ForwardIterator __first,
			    _ForwardIterator __last, std::forward_iterator_tag);

      // Called by insert(p,n,x), and the range insert when it turns out to be
      // the same thing.  Can use fill functions in optimal situations,
      // otherwise passes off to insert_aux(p,n,x).
      void
      _M_fill_insert(iterator __pos, size_type __n, const value_type& __x);

      // called by insert(p,x)
#if __cplusplus < 201103L
      iterator
      _M_insert_aux(iterator __pos, const value_type& __x);
#else
      template<typename... _Args>
	iterator
	_M_insert_aux(iterator __pos, _Args&&... __args);
#endif

      // called by insert(p,n,x) via fill_insert
      void
      _M_insert_aux(iterator __pos, size_type __n, const value_type& __x);

      // called by range_insert_aux for forward iterators
      template<typename _ForwardIterator>
	void
	_M_insert_aux(iterator __pos,
		      _ForwardIterator __first, _ForwardIterator __last,
		      size_type __n);


      // Internal erase functions follow.

      void
      _M_destroy_data_aux(iterator __first, iterator __last);

      // Called by ~deque().
      // NB: Doesn't deallocate the nodes.
      template<typename _Alloc1>
	void
	_M_destroy_data(iterator __first, iterator __last, const _Alloc1&)
	{ _M_destroy_data_aux(__first, __last); }

      void
      _M_destroy_data(iterator __first, iterator __last,
		      const std::allocator<_Tp>&)
      {
	if (!__has_trivial_destructor(value_type))
	  _M_destroy_data_aux(__first, __last);
      }

      // Called by erase(q1, q2).
      void
      _M_erase_at_begin(iterator __pos)
      {
	_M_destroy_data(begin(), __pos, _M_get_Tp_allocator());
	_M_destroy_nodes(this->_M_impl._M_start._M_node, __pos._M_node);
	this->_M_impl._M_start = __pos;
      }

      // Called by erase(q1, q2), resize(), clear(), _M_assign_aux,
      // _M_fill_assign, operator=.
      void
      _M_erase_at_end(iterator __pos)
      {
	_M_destroy_data(__pos, end(), _M_get_Tp_allocator());
	_M_destroy_nodes(__pos._M_node + 1,
			 this->_M_impl._M_finish._M_node + 1);
	this->_M_impl._M_finish = __pos;
      }

      iterator
      _M_erase(iterator __pos);

      iterator
      _M_erase(iterator __first, iterator __last);

#if __cplusplus >= 201103L
      // Called by resize(sz).
      void
      _M_default_append(size_type __n);

      bool
      _M_shrink_to_fit();
#endif

      //@{
      /// Memory-handling helpers for the previous internal insert functions.
      iterator
      _M_reserve_elements_at_front(size_type __n)
      {
	const size_type __vacancies = this->_M_impl._M_start._M_cur
				      - this->_M_impl._M_start._M_first;
	if (__n > __vacancies)
	  _M_new_elements_at_front(__n - __vacancies);
	return this->_M_impl._M_start - difference_type(__n);
      }

      iterator
      _M_reserve_elements_at_back(size_type __n)
      {
	const size_type __vacancies = (this->_M_impl._M_finish._M_last
				       - this->_M_impl._M_finish._M_cur) - 1;
	if (__n > __vacancies)
	  _M_new_elements_at_back(__n - __vacancies);
	return this->_M_impl._M_finish + difference_type(__n);
      }

      void
      _M_new_elements_at_front(size_type __new_elements);

      void
      _M_new_elements_at_back(size_type __new_elements);
      //@}


      //@{
      /**
       *  @brief Memory-handling helpers for the major %map.
       *
       *  Makes sure the _M_map has space for new nodes.  Does not
       *  actually add the nodes.  Can invalidate _M_map pointers.
       *  (And consequently, %deque iterators.)
       */
      void
      _M_reserve_map_at_back(size_type __nodes_to_add = 1)
      {
	if (__nodes_to_add + 1 > this->_M_impl._M_map_size
	    - (this->_M_impl._M_finish._M_node - this->_M_impl._M_map))
	  _M_reallocate_map(__nodes_to_add, false);
      }

      void
      _M_reserve_map_at_front(size_type __nodes_to_add = 1)
      {
	if (__nodes_to_add > size_type(this->_M_impl._M_start._M_node
				       - this->_M_impl._M_map)) // map是fixed-size的，需要判断前面未allocate的node是否足够
	  _M_reallocate_map(__nodes_to_add, true); // 见deque.tcc
      }

      void
      _M_reallocate_map(size_type __nodes_to_add, bool __add_at_front);
      //@}

#if __cplusplus >= 201103L
      // Constant-time, nothrow move assignment when source object's memory
      // can be moved because the allocators are equal.
      void
      _M_move_assign1(deque&& __x, /* always equal: */ true_type) noexcept
      {
	this->_M_impl._M_swap_data(__x._M_impl);
	__x.clear();
	std::__alloc_on_move(_M_get_Tp_allocator(), __x._M_get_Tp_allocator());
      }

      // When the allocators are not equal the operation could throw, because
      // we might need to allocate a new map for __x after moving from it
      // or we might need to allocate new elements for *this.
      void
      _M_move_assign1(deque&& __x, /* always equal: */ false_type)
      {
	constexpr bool __move_storage =
	  _Alloc_traits::_S_propagate_on_move_assign();
	_M_move_assign2(std::move(__x), __bool_constant<__move_storage>());
      }

      // Destroy all elements and deallocate all memory, then replace
      // with elements created from __args.
      template<typename... _Args>
      void
      _M_replace_map(_Args&&... __args)
      {
	// Create new data first, so if allocation fails there are no effects.
	deque __newobj(std::forward<_Args>(__args)...);
	// Free existing storage using existing allocator.
	clear();
	_M_deallocate_node(*begin()._M_node); // one node left after clear()
	_M_deallocate_map(this->_M_impl._M_map, this->_M_impl._M_map_size);
	this->_M_impl._M_map = nullptr;
	this->_M_impl._M_map_size = 0;
	// Take ownership of replacement memory.
	this->_M_impl._M_swap_data(__newobj._M_impl);
      }

      // Do move assignment when the allocator propagates.
      void
      _M_move_assign2(deque&& __x, /* propagate: */ true_type)
      {
	// Make a copy of the original allocator state.
	auto __alloc = __x._M_get_Tp_allocator();
	// The allocator propagates so storage can be moved from __x,
	// leaving __x in a valid empty state with a moved-from allocator.
	_M_replace_map(std::move(__x));
	// Move the corresponding allocator state too.
	_M_get_Tp_allocator() = std::move(__alloc);
      }

      // Do move assignment when it may not be possible to move source
      // object's memory, resulting in a linear-time operation.
      void
      _M_move_assign2(deque&& __x, /* propagate: */ false_type)
      {
	if (__x._M_get_Tp_allocator() == this->_M_get_Tp_allocator())
	  {
	    // The allocators are equal so storage can be moved from __x,
	    // leaving __x in a valid empty state with its current allocator.
	    _M_replace_map(std::move(__x), __x.get_allocator());
	  }
	else
	  {
	    // The rvalue's allocator cannot be moved and is not equal,
	    // so we need to individually move each element.
	    _M_assign_aux(std::__make_move_if_noexcept_iterator(__x.begin()),
			  std::__make_move_if_noexcept_iterator(__x.end()),
			  std::random_access_iterator_tag());
	    __x.clear();
	  }
      }
#endif
    };

#if __cpp_deduction_guides >= 201606
  template<typename _InputIterator, typename _ValT
	     = typename iterator_traits<_InputIterator>::value_type,
	   typename _Allocator = allocator<_ValT>,
	   typename = _RequireInputIter<_InputIterator>,
	   typename = _RequireAllocator<_Allocator>>
    deque(_InputIterator, _InputIterator, _Allocator = _Allocator())
      -> deque<_ValT, _Allocator>;
#endif

  /**
   *  @brief  Deque equality comparison.
   *  @param  __x  A %deque.
   *  @param  __y  A %deque of the same type as @a __x.
   *  @return  True iff the size and elements of the deques are equal.
   *
   *  This is an equivalence relation.  It is linear in the size of the
   *  deques.  Deques are considered equivalent if their sizes are equal,
   *  and if corresponding elements compare equal.
  */
  template<typename _Tp, typename _Alloc>
    inline bool
    operator==(const deque<_Tp, _Alloc>& __x,
                         const deque<_Tp, _Alloc>& __y)
    { return __x.size() == __y.size()
	     && std::equal(__x.begin(), __x.end(), __y.begin()); }

  /**
   *  @brief  Deque ordering relation.
   *  @param  __x  A %deque.
   *  @param  __y  A %deque of the same type as @a __x.
   *  @return  True iff @a x is lexicographically less than @a __y.
   *
   *  This is a total ordering relation.  It is linear in the size of the
   *  deques.  The elements must be comparable with @c <.
   *
   *  See std::lexicographical_compare() for how the determination is made.
  */
  template<typename _Tp, typename _Alloc>
    inline bool
    operator<(const deque<_Tp, _Alloc>& __x,
	      const deque<_Tp, _Alloc>& __y)
    { return std::lexicographical_compare(__x.begin(), __x.end(),
					  __y.begin(), __y.end()); }

  /// Based on operator==
  template<typename _Tp, typename _Alloc>
    inline bool
    operator!=(const deque<_Tp, _Alloc>& __x,
	       const deque<_Tp, _Alloc>& __y)
    { return !(__x == __y); }

  /// Based on operator<
  template<typename _Tp, typename _Alloc>
    inline bool
    operator>(const deque<_Tp, _Alloc>& __x,
	      const deque<_Tp, _Alloc>& __y)
    { return __y < __x; }

  /// Based on operator<
  template<typename _Tp, typename _Alloc>
    inline bool
    operator<=(const deque<_Tp, _Alloc>& __x,
	       const deque<_Tp, _Alloc>& __y)
    { return !(__y < __x); }

  /// Based on operator<
  template<typename _Tp, typename _Alloc>
    inline bool
    operator>=(const deque<_Tp, _Alloc>& __x,
	       const deque<_Tp, _Alloc>& __y)
    { return !(__x < __y); }

  /// See std::deque::swap().
  template<typename _Tp, typename _Alloc>
    inline void
    swap(deque<_Tp,_Alloc>& __x, deque<_Tp,_Alloc>& __y)
    _GLIBCXX_NOEXCEPT_IF(noexcept(__x.swap(__y)))
    { __x.swap(__y); }

#undef _GLIBCXX_DEQUE_BUF_SIZE

_GLIBCXX_END_NAMESPACE_CONTAINER

#if __cplusplus >= 201103L
  // std::allocator is safe, but it is not the only allocator
  // for which this is valid.
  template<class _Tp>
    struct __is_bitwise_relocatable<_GLIBCXX_STD_C::deque<_Tp>>
    : true_type { };
#endif
