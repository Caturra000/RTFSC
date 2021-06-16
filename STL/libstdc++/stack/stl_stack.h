/** @file bits/stl_stack.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly. @headername{stack}
 */

// stack并没有什么特别的，它是作为适配器（not a true container），函数均为封装函数
// 底层实现完全依赖于typename _Sequence，默认为deque
// 其内部成员也只有_Sequence c (underlying container)
// Question. 从直觉上来看，vector似乎比deque更适合作为stack的容器（从实现可以看出，deque的插入复杂度常数是比较大的），为什么还是选了deque
// 可能的答案：vector.push_back是均摊O(1), deque.push是确定的O(1)

#ifndef _STL_STACK_H
#define _STL_STACK_H 1

#include <bits/concept_check.h>
#include <debug/debug.h>
#if __cplusplus >= 201103L
# include <bits/uses_allocator.h>
#endif

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  /**
   *  @brief  A standard container giving FILO behavior.
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
   *  This is not a true container, but an @e adaptor.  It holds                        // 关键说明
   *  another container, and provides a wrapper interface to that
   *  container.  The wrapper is what enforces strict
   *  first-in-last-out %stack behavior.
   *
   *  The second template parameter defines the type of the underlying
   *  sequence/container.  It defaults to std::deque, but it can be
   *  any type that supports @c back, @c push_back, and @c pop_back,
   *  such as std::list, std::vector, or an appropriate user-defined
   *  type.
   *
   *  Members not found in @a normal containers are @c container_type,
   *  which is a typedef for the second Sequence parameter, and @c
   *  push, @c pop, and @c top, which are standard %stack/FILO
   *  operations.
  */
  template<typename _Tp, typename _Sequence = deque<_Tp> >
    class stack
    {
#ifdef _GLIBCXX_CONCEPT_CHECKS
      // concept requirements
      typedef typename _Sequence::value_type _Sequence_value_type;
# if __cplusplus < 201103L
      __glibcxx_class_requires(_Tp, _SGIAssignableConcept)
      __glibcxx_class_requires(_Sequence, _BackInsertionSequenceConcept)
# endif
      __glibcxx_class_requires2(_Tp, _Sequence_value_type, _SameTypeConcept)
#endif

      template<typename _Tp1, typename _Seq1>
	friend bool
	operator==(const stack<_Tp1, _Seq1>&, const stack<_Tp1, _Seq1>&);

      template<typename _Tp1, typename _Seq1>
	friend bool
	operator<(const stack<_Tp1, _Seq1>&, const stack<_Tp1, _Seq1>&);

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
      typedef typename _Sequence::value_type		value_type;
      typedef typename _Sequence::reference		reference;
      typedef typename _Sequence::const_reference	const_reference;
      typedef typename _Sequence::size_type		size_type;
      typedef	       _Sequence			container_type;

    protected:
      //  See queue::c for notes on this name.
      _Sequence c;

    public:
      // XXX removed old def ctor, added def arg to this one to match 14882
      /**
       *  @brief  Default constructor creates no elements.
       */
#if __cplusplus < 201103L
      explicit
      stack(const _Sequence& __c = _Sequence())
      : c(__c) { }
#else
      template<typename _Seq = _Sequence, typename _Requires = typename
	       enable_if<is_default_constructible<_Seq>::value>::type>
	stack()
	: c() { }

      explicit
      stack(const _Sequence& __c)
      : c(__c) { }

      explicit
      stack(_Sequence&& __c)
      : c(std::move(__c)) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	explicit
	stack(const _Alloc& __a)
	: c(__a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	stack(const _Sequence& __c, const _Alloc& __a)
	: c(__c, __a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	stack(_Sequence&& __c, const _Alloc& __a)
	: c(std::move(__c), __a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	stack(const stack& __q, const _Alloc& __a)
	: c(__q.c, __a) { }

      template<typename _Alloc, typename _Requires = _Uses<_Alloc>>
	stack(stack&& __q, const _Alloc& __a)
	: c(std::move(__q.c), __a) { }
#endif

      /**
       *  Returns true if the %stack is empty.
       */
      _GLIBCXX_NODISCARD bool
      empty() const
      { return c.empty(); }

      /**  Returns the number of elements in the %stack.  */
      size_type
      size() const
      { return c.size(); }

      /**
       *  Returns a read/write reference to the data at the first
       *  element of the %stack.
       */
      reference
      top()
      {
	__glibcxx_requires_nonempty();
	return c.back();
      }

      /**
       *  Returns a read-only (constant) reference to the data at the first
       *  element of the %stack.
       */
      const_reference
      top() const
      {
	__glibcxx_requires_nonempty();
	return c.back();
      }

      /**
       *  @brief  Add data to the top of the %stack.
       *  @param  __x  Data to be added.
       *
       *  This is a typical %stack operation.  The function creates an
       *  element at the top of the %stack and assigns the given data
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
       *  This is a typical %stack operation.  It shrinks the %stack
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
	c.pop_back();
      }

#if __cplusplus >= 201103L
      void
      swap(stack& __s)
#if __cplusplus > 201402L || !defined(__STRICT_ANSI__) // c++1z or gnu++11
      noexcept(__is_nothrow_swappable<_Sequence>::value)
#else
      noexcept(__is_nothrow_swappable<_Tp>::value)
#endif
      {
	using std::swap;
	swap(c, __s.c);
      }
#endif // __cplusplus >= 201103L
    };

#if __cpp_deduction_guides >= 201606
  template<typename _Container,
	   typename = _RequireNotAllocator<_Container>>
    stack(_Container) -> stack<typename _Container::value_type, _Container>;

  template<typename _Container, typename _Allocator,
	   typename = _RequireNotAllocator<_Container>,
	   typename = _RequireAllocator<_Allocator>>
    stack(_Container, _Allocator)
    -> stack<typename _Container::value_type, _Container>;
#endif

  /**
   *  @brief  Stack equality comparison.
   *  @param  __x  A %stack.
   *  @param  __y  A %stack of the same type as @a __x.
   *  @return  True iff the size and elements of the stacks are equal.
   *
   *  This is an equivalence relation.  Complexity and semantics
   *  depend on the underlying sequence type, but the expected rules
   *  are: this relation is linear in the size of the sequences, and
   *  stacks are considered equivalent if their sequences compare
   *  equal.
  */
  template<typename _Tp, typename _Seq>
    inline bool
    operator==(const stack<_Tp, _Seq>& __x, const stack<_Tp, _Seq>& __y)
    { return __x.c == __y.c; }

  /**
   *  @brief  Stack ordering relation.
   *  @param  __x  A %stack.
   *  @param  __y  A %stack of the same type as @a x.
   *  @return  True iff @a x is lexicographically less than @a __y.
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
    operator<(const stack<_Tp, _Seq>& __x, const stack<_Tp, _Seq>& __y)
    { return __x.c < __y.c; }

  /// Based on operator==
  template<typename _Tp, typename _Seq>
    inline bool
    operator!=(const stack<_Tp, _Seq>& __x, const stack<_Tp, _Seq>& __y)
    { return !(__x == __y); }

  /// Based on operator<
  template<typename _Tp, typename _Seq>
    inline bool
    operator>(const stack<_Tp, _Seq>& __x, const stack<_Tp, _Seq>& __y)
    { return __y < __x; }

  /// Based on operator<
  template<typename _Tp, typename _Seq>
    inline bool
    operator<=(const stack<_Tp, _Seq>& __x, const stack<_Tp, _Seq>& __y)
    { return !(__y < __x); }

  /// Based on operator<
  template<typename _Tp, typename _Seq>
    inline bool
    operator>=(const stack<_Tp, _Seq>& __x, const stack<_Tp, _Seq>& __y)
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
    swap(stack<_Tp, _Seq>& __x, stack<_Tp, _Seq>& __y)
    noexcept(noexcept(__x.swap(__y)))
    { __x.swap(__y); }

  template<typename _Tp, typename _Seq, typename _Alloc>
    struct uses_allocator<stack<_Tp, _Seq>, _Alloc>
    : public uses_allocator<_Seq, _Alloc>::type { };
#endif // __cplusplus >= 201103L

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace

#endif /* _STL_STACK_H */
