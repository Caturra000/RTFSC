/** @file bits/hashtable_policy.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly.
 *  @headername{unordered_map,unordered_set}
 */

// 访问/插入流程： ##flag #0
// 1. 位于_Map_base，但是_Map_base只是负责流程，并没有实际实现
// 2. 通过CRTP把this转型为派生类__hashtable
// 3. 实际流程：
//     _M_hash_code() -> _M_bucket_index() -> _M_find_node() -------> return _M_v().second
//                                                  ↓                     ↑
//                                            _M_allocate_node()          |
//                                                  ↓                     |
//                                            _M_insert_unique_node() ----
// 就接口而言是非常直观的
// 其中，
// _M_hash_code()位于_Hash_code_base
// _M_bucket_index()位于_Hash_code_base
// _M_find_node()位于_Hashtable
// _M_allocate_node()位于_Hashtable_alloc
// _M_insert_unique_node()位于_Hashtable

// 观察unordered_map.h，它具体用到的policy类为
//  template<bool _Cache>
//    using __umap_traits = __detail::_Hashtable_traits<_Cache, false, true>;
// template<typename _Key,
// 	   typename _Tp,
// 	   typename _Hash = hash<_Key>,
// 	   typename _Pred = std::equal_to<_Key>,
// 	   typename _Alloc = std::allocator<std::pair<const _Key, _Tp> >,
// 	   typename _Tr = __umap_traits<__cache_default<_Key, _Hash>::value>>
//     using __umap_hashtable = _Hashtable<_Key, std::pair<const _Key, _Tp>,
//                                         _Alloc, __detail::_Select1st,
// 				        _Pred, _Hash,
// 				        __detail::_Mod_range_hashing,
// 				        __detail::_Default_ranged_hash,
// 				        __detail::_Prime_rehash_policy, _Tr>;
//
//  template<typename _Key, typename _Tp,
// 	   typename _Hash = hash<_Key>,
// 	   typename _Pred = equal_to<_Key>,
// 	   typename _Alloc = allocator<std::pair<const _Key, _Tp>>>
//     class unordered_map
//     {
//       typedef __umap_hashtable<_Key, _Tp, _Hash, _Pred, _Alloc>  _Hashtable;
//       _Hashtable _M_h;
// ........略
//
// template<typename _Key, typename _Value, typename _Alloc,
// 	   typename _ExtractKey, typename _Equal,
// 	   typename _H1, typename _H2, typename _Hash,
// 	   typename _RehashPolicy, typename _Traits>
//     class _Hashtable
// .......
// 也就是默认情况下，
// _ExtractKey = __detail::_Select1st
// _Equal = std::equal_to<_Key>
// _H1 = hash<_Key>
// _H2 = __detail::_Mod_range_hashing
// _Hash = __detail::_Default_ranged_hash
// _RehashPolicy = __detail::_Prime_rehash_policy
// _Traits = __detail::_Hashtable_traits<__cache_default<_Key, __detail::_Default_ranged_hash>::value>, false, true>

// _M_hash_code()跟踪
// 太长不看版本：默认就是调用_H1()，既std::hash
// 长篇大论版本：
// 前面总结的插入流程是：
// 当unordered_map调用operator[]时，就是直接调用_Hashtable::operator[]
// 而_Hashtable本身并没有实现，而是继承自_Map_base
// 但是_Map_base::operator[]中对_M_hash_code的调用是把this转型为_Hashtable::_M_hash_code()
// 重新回到_Hashtable，找到它继承的另一个CRTP接口_Hashtable_base ##flag #1
// 可以发现_Hashtable_base继承自_Hash_code_base
// _Hash_code_base针对_Hash = _Default_ranged_hash的偏特化实现（不管是否有__cache_hash_code）为
//      __hash_code
//       _M_hash_code(const _Key& __k) const
//       {
// 	    return _M_h1()(__k); // 省略static_assert
//       }

// _M_bucket_index()跟踪
// 同一个_Hash_code_base，调用_M_h2()(__c, __n)

// _M_find_node()跟踪
// 位于_Hashtable
// 返回_M_find_before_node()->_M_nxt
// （如果没有，则返回nullptr）
// 先不看这个过程，因为不明确bucket是怎么装的，目前流程可认为是从构造好hashtable后直接插入，必返回nullptr

// _M_allocate_node()略，就是简单的allocate

// _M_insert_unique_node()跟踪
// 位于位于_Hashtable
// 需要用到_M_rehash_policy，作用为
//     1. 判断是否需要rehash（通过_M_need_rehash）
//     2. 具体rehash流程：_M_rehash和重定位__bkt（通过_M_bucket_index）
// 插入流程走_M_store_code _M_insert_bucket_begin
//     _M_store_code位于hash_code_base，就是存储hashcode的setter，因为可能没有，所以用函数封装一下方便重载
//     _M_insert_bucket_begin（只分析找不到bucket的行为）
//         分支判断蛮奇怪的，不需要判断_M_buckets为空的情况？构造完成后应该只是一个nullptr，
//             可能是进入_M_buckets[__bkt]时由于bucket_count=1，必然__bkt=0，得到nullptr==0跳过该分支，只是用了一种我不太习惯的写法
//         首次插入会走else分支，插入的__node会成为_M_before_begin._M_nxt（原来的next称为former）
//             如果former是存在的，那么former原来所在的bucket也要指向__node（很显然首次插入没有，另外这里的bucket下标要经过hash_code_base取模）
//         bucket[__bkt]指向before begin
// 这个插入的微妙在于，指定的__bkt插入的却是begin before，begin before的next才是被插入（头插）的结点
// 对于刚插入的情况，bucket数目显然不会增加
// 本来构造好的_M_buckets是nullptr，插入后 _M_buckets[__bkt] = &_M_before_begin（一个预设节点）; __bkt=0 此时_M_buckets才含有_M_before_begin和插入的node(before begin.next)
// （好累啊，写的什么鬼，直接debug跟踪算了吧）

// rehash默认使用_Prime_rehash_policy
// 然而这个没法在hashtable和hashtable_policy文件找到 ##flag #2
// 查了gcc的changelog，发现它的部分实现移到了hashtable_c++0x.cc，已加到本文件中


#ifndef _HASHTABLE_POLICY_H
#define _HASHTABLE_POLICY_H 1

#include <tuple>		// for std::tuple, std::forward_as_tuple
#include <limits>		// for std::numeric_limits
#include <bits/stl_algobase.h>	// for std::min.

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    class _Hashtable;

namespace __detail
{
  /**
   *  @defgroup hashtable-detail Base and Implementation Classes
   *  @ingroup unordered_associative_containers
   *  @{
   */
  template<typename _Key, typename _Value,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash, typename _Traits>
    struct _Hashtable_base;

  // Helper function: return distance(first, last) for forward
  // iterators, or 0/1 for input iterators.
  template<class _Iterator>
    inline typename std::iterator_traits<_Iterator>::difference_type
    __distance_fw(_Iterator __first, _Iterator __last,
		  std::input_iterator_tag)
    { return __first != __last ? 1 : 0; }

  template<class _Iterator>
    inline typename std::iterator_traits<_Iterator>::difference_type
    __distance_fw(_Iterator __first, _Iterator __last,
		  std::forward_iterator_tag)
    { return std::distance(__first, __last); }

  template<class _Iterator>
    inline typename std::iterator_traits<_Iterator>::difference_type
    __distance_fw(_Iterator __first, _Iterator __last)
    { return __distance_fw(__first, __last,
			   std::__iterator_category(__first)); }

  struct _Identity
  {
    template<typename _Tp>
      _Tp&&
      operator()(_Tp&& __x) const
      { return std::forward<_Tp>(__x); }
  };

  struct _Select1st
  {
    template<typename _Tp>
      auto
      operator()(_Tp&& __x) const
      -> decltype(std::get<0>(std::forward<_Tp>(__x)))
      { return std::get<0>(std::forward<_Tp>(__x)); }
  };

  template<typename _NodeAlloc>
    struct _Hashtable_alloc;

  // Functor recycling a pool of nodes and using allocation once the pool is
  // empty.
  template<typename _NodeAlloc>
    struct _ReuseOrAllocNode
    {
    private:
      using __node_alloc_type = _NodeAlloc;
      using __hashtable_alloc = _Hashtable_alloc<__node_alloc_type>;
      using __node_alloc_traits =
	typename __hashtable_alloc::__node_alloc_traits;
      using __node_type = typename __hashtable_alloc::__node_type;

    public:
      _ReuseOrAllocNode(__node_type* __nodes, __hashtable_alloc& __h)
	: _M_nodes(__nodes), _M_h(__h) { }
      _ReuseOrAllocNode(const _ReuseOrAllocNode&) = delete;

      ~_ReuseOrAllocNode()
      { _M_h._M_deallocate_nodes(_M_nodes); }

      template<typename _Arg>
	__node_type*
	operator()(_Arg&& __arg) const
	{
	  if (_M_nodes)
	    {
	      __node_type* __node = _M_nodes;
	      _M_nodes = _M_nodes->_M_next();
	      __node->_M_nxt = nullptr;
	      auto& __a = _M_h._M_node_allocator();
	      __node_alloc_traits::destroy(__a, __node->_M_valptr());
	      __try
		{
		  __node_alloc_traits::construct(__a, __node->_M_valptr(),
						 std::forward<_Arg>(__arg));
		}
	      __catch(...)
		{
		  _M_h._M_deallocate_node_ptr(__node);
		  __throw_exception_again;
		}
	      return __node;
	    }
	  return _M_h._M_allocate_node(std::forward<_Arg>(__arg));
	}

    private:
      mutable __node_type* _M_nodes;
      __hashtable_alloc& _M_h;
    };

  // Functor similar to the previous one but without any pool of nodes to
  // recycle.
  template<typename _NodeAlloc>
    struct _AllocNode
    {
    private:
      using __hashtable_alloc = _Hashtable_alloc<_NodeAlloc>;
      using __node_type = typename __hashtable_alloc::__node_type;

    public:
      _AllocNode(__hashtable_alloc& __h)
	: _M_h(__h) { }

      template<typename _Arg>
	__node_type*
	operator()(_Arg&& __arg) const
	{ return _M_h._M_allocate_node(std::forward<_Arg>(__arg)); }

    private:
      __hashtable_alloc& _M_h;
    };

  // Auxiliary types used for all instantiations of _Hashtable nodes
  // and iterators.

  /**
   *  struct _Hashtable_traits
   *
   *  Important traits for hash tables.
   *
   *  @tparam _Cache_hash_code  Boolean value. True if the value of
   *  the hash function is stored along with the value. This is a
   *  time-space tradeoff.  Storing it may improve lookup speed by
   *  reducing the number of times we need to call the _Equal
   *  function.
   *
   *  @tparam _Constant_iterators  Boolean value. True if iterator and
   *  const_iterator are both constant iterator types. This is true
   *  for unordered_set and unordered_multiset, false for
   *  unordered_map and unordered_multimap.
   *
   *  @tparam _Unique_keys  Boolean value. True if the return value
   *  of _Hashtable::count(k) is always at most one, false if it may
   *  be an arbitrary number. This is true for unordered_set and
   *  unordered_map, false for unordered_multiset and
   *  unordered_multimap.
   */
  template<bool _Cache_hash_code, bool _Constant_iterators, bool _Unique_keys>
    struct _Hashtable_traits
    {
      using __hash_cached = __bool_constant<_Cache_hash_code>;
      using __constant_iterators = __bool_constant<_Constant_iterators>;
      using __unique_keys = __bool_constant<_Unique_keys>;
    };

  /**
   *  struct _Hash_node_base
   *
   *  Nodes, used to wrap elements stored in the hash table.  A policy
   *  template parameter of class template _Hashtable controls whether
   *  nodes also store a hash code. In some cases (e.g. strings) this
   *  may be a performance win.
   */
  struct _Hash_node_base
  {
    _Hash_node_base* _M_nxt;

    _Hash_node_base() noexcept : _M_nxt() { }

    _Hash_node_base(_Hash_node_base* __next) noexcept : _M_nxt(__next) { }
  };

  /**
   *  struct _Hash_node_value_base
   *
   *  Node type with the value to store.
   */
  template<typename _Value>
    struct _Hash_node_value_base : _Hash_node_base
    {
      typedef _Value value_type;

      __gnu_cxx::__aligned_buffer<_Value> _M_storage;

      _Value*
      _M_valptr() noexcept
      { return _M_storage._M_ptr(); }

      const _Value*
      _M_valptr() const noexcept
      { return _M_storage._M_ptr(); }

      _Value&
      _M_v() noexcept
      { return *_M_valptr(); }

      const _Value&
      _M_v() const noexcept
      { return *_M_valptr(); }
    };

  /**
   *  Primary template struct _Hash_node.
   */
  template<typename _Value, bool _Cache_hash_code>
    struct _Hash_node;

  /**
   *  Specialization for nodes with caches, struct _Hash_node.
   *
   *  Base class is __detail::_Hash_node_value_base.
   */
  template<typename _Value>
    struct _Hash_node<_Value, true> : _Hash_node_value_base<_Value>
    {
      std::size_t  _M_hash_code;

      _Hash_node*
      _M_next() const noexcept
      { return static_cast<_Hash_node*>(this->_M_nxt); }
    };

  /**
   *  Specialization for nodes without caches, struct _Hash_node.
   *
   *  Base class is __detail::_Hash_node_value_base.
   */
  template<typename _Value>
    struct _Hash_node<_Value, false> : _Hash_node_value_base<_Value>
    {
      _Hash_node*
      _M_next() const noexcept
      { return static_cast<_Hash_node*>(this->_M_nxt); }
    };

  /// Base class for node iterators.
  template<typename _Value, bool _Cache_hash_code>
    struct _Node_iterator_base
    {
      using __node_type = _Hash_node<_Value, _Cache_hash_code>;

      __node_type*  _M_cur;

      _Node_iterator_base(__node_type* __p) noexcept
      : _M_cur(__p) { }

      void
      _M_incr() noexcept
      { _M_cur = _M_cur->_M_next(); }
    };

  template<typename _Value, bool _Cache_hash_code>
    inline bool
    operator==(const _Node_iterator_base<_Value, _Cache_hash_code>& __x,
	       const _Node_iterator_base<_Value, _Cache_hash_code >& __y)
    noexcept
    { return __x._M_cur == __y._M_cur; }

  template<typename _Value, bool _Cache_hash_code>
    inline bool
    operator!=(const _Node_iterator_base<_Value, _Cache_hash_code>& __x,
	       const _Node_iterator_base<_Value, _Cache_hash_code>& __y)
    noexcept
    { return __x._M_cur != __y._M_cur; }

  /// Node iterators, used to iterate through all the hashtable.
  template<typename _Value, bool __constant_iterators, bool __cache>
    struct _Node_iterator
    : public _Node_iterator_base<_Value, __cache>
    {
    private:
      using __base_type = _Node_iterator_base<_Value, __cache>;
      using __node_type = typename __base_type::__node_type;

    public:
      typedef _Value					value_type;
      typedef std::ptrdiff_t				difference_type;
      typedef std::forward_iterator_tag			iterator_category;

      using pointer = typename std::conditional<__constant_iterators,
						const _Value*, _Value*>::type;

      using reference = typename std::conditional<__constant_iterators,
						  const _Value&, _Value&>::type;

      _Node_iterator() noexcept
      : __base_type(0) { }

      explicit
      _Node_iterator(__node_type* __p) noexcept
      : __base_type(__p) { }

      reference
      operator*() const noexcept
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const noexcept
      { return this->_M_cur->_M_valptr(); }

      _Node_iterator&
      operator++() noexcept
      {
	this->_M_incr();
	return *this;
      }

      _Node_iterator
      operator++(int) noexcept
      {
	_Node_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  /// Node const_iterators, used to iterate through all the hashtable.
  template<typename _Value, bool __constant_iterators, bool __cache>
    struct _Node_const_iterator
    : public _Node_iterator_base<_Value, __cache>
    {
    private:
      using __base_type = _Node_iterator_base<_Value, __cache>;
      using __node_type = typename __base_type::__node_type;

    public:
      typedef _Value					value_type;
      typedef std::ptrdiff_t				difference_type;
      typedef std::forward_iterator_tag			iterator_category;

      typedef const _Value*				pointer;
      typedef const _Value&				reference;

      _Node_const_iterator() noexcept
      : __base_type(0) { }

      explicit
      _Node_const_iterator(__node_type* __p) noexcept
      : __base_type(__p) { }

      _Node_const_iterator(const _Node_iterator<_Value, __constant_iterators,
			   __cache>& __x) noexcept
      : __base_type(__x._M_cur) { }

      reference
      operator*() const noexcept
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const noexcept
      { return this->_M_cur->_M_valptr(); }

      _Node_const_iterator&
      operator++() noexcept
      {
	this->_M_incr();
	return *this;
      }

      _Node_const_iterator
      operator++(int) noexcept
      {
	_Node_const_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  // Many of class template _Hashtable's template parameters are policy
  // classes.  These are defaults for the policies.

  /// Default range hashing function: use division to fold a large number
  /// into the range [0, N).
  struct _Mod_range_hashing
  {
    typedef std::size_t first_argument_type;
    typedef std::size_t second_argument_type;
    typedef std::size_t result_type;

    result_type
    operator()(first_argument_type __num,
	       second_argument_type __den) const noexcept
    { return __num % __den; }
  };

  /// Default ranged hash function H.  In principle it should be a
  /// function object composed from objects of type H1 and H2 such that
  /// h(k, N) = h2(h1(k), N), but that would mean making extra copies of
  /// h1 and h2.  So instead we'll just use a tag to tell class template
  /// hashtable to do that composition.
  struct _Default_ranged_hash { };

  /// Default value for rehash policy.  Bucket size is (usually) the
  /// smallest prime that keeps the load factor small enough.
  struct _Prime_rehash_policy
  {
    using __has_load_factor = std::true_type;

    _Prime_rehash_policy(float __z = 1.0) noexcept
    : _M_max_load_factor(__z), _M_next_resize(0) { }

    float
    max_load_factor() const noexcept
    { return _M_max_load_factor; }

    // Return a bucket size no smaller than n.
    std::size_t
    _M_next_bkt(std::size_t __n) const;

    // Return a bucket count appropriate for n elements
    std::size_t
    _M_bkt_for_elements(std::size_t __n) const
    { return __builtin_ceil(__n / (long double)_M_max_load_factor); }

    // __n_bkt is current bucket count, __n_elt is current element count,
    // and __n_ins is number of elements to be inserted.  Do we need to
    // increase bucket count?  If so, return make_pair(true, n), where n
    // is the new bucket count.  If not, return make_pair(false, 0).
    std::pair<bool, std::size_t>
    _M_need_rehash(std::size_t __n_bkt, std::size_t __n_elt,  // ##flag #2
		   std::size_t __n_ins) const;

    typedef std::size_t _State;

    _State
    _M_state() const
    { return _M_next_resize; }

    void
    _M_reset() noexcept
    { _M_next_resize = 0; }

    void
    _M_reset(_State __state)
    { _M_next_resize = __state; }

    static const std::size_t _S_growth_factor = 2;

    float		_M_max_load_factor;
    mutable std::size_t	_M_next_resize;
  };

  /// Range hashing function assuming that second arg is a power of 2.
  struct _Mask_range_hashing
  {
    typedef std::size_t first_argument_type;
    typedef std::size_t second_argument_type;
    typedef std::size_t result_type;

    result_type
    operator()(first_argument_type __num,
	       second_argument_type __den) const noexcept
    { return __num & (__den - 1); }
  };

  /// Compute closest power of 2 not less than __n
  inline std::size_t
  __clp2(std::size_t __n) noexcept
  {
    // Equivalent to return __n ? std::ceil2(__n) : 0;
    if (__n < 2)
      return __n;
    const unsigned __lz = sizeof(size_t) > sizeof(long)
      ? __builtin_clzll(__n - 1ull)
      : __builtin_clzl(__n - 1ul);
    // Doing two shifts avoids undefined behaviour when __lz == 0.
    return (size_t(1) << (numeric_limits<size_t>::digits - __lz - 1)) << 1;
  }

  /// Rehash policy providing power of 2 bucket numbers. Avoids modulo
  /// operations.
  struct _Power2_rehash_policy
  {
    using __has_load_factor = std::true_type;

    _Power2_rehash_policy(float __z = 1.0) noexcept
    : _M_max_load_factor(__z), _M_next_resize(0) { }

    float
    max_load_factor() const noexcept
    { return _M_max_load_factor; }

    // Return a bucket size no smaller than n (as long as n is not above the
    // highest power of 2).
    std::size_t
    _M_next_bkt(std::size_t __n) noexcept
    {
      const auto __max_width = std::min<size_t>(sizeof(size_t), 8);
      const auto __max_bkt = size_t(1) << (__max_width * __CHAR_BIT__ - 1);
      std::size_t __res = __clp2(__n);

      if (__res == __n)
	__res <<= 1;

      if (__res == 0)
	__res = __max_bkt;

      if (__res == __max_bkt)
	// Set next resize to the max value so that we never try to rehash again
	// as we already reach the biggest possible bucket number.
	// Note that it might result in max_load_factor not being respected.
	_M_next_resize = std::size_t(-1);
      else
	_M_next_resize
	  = __builtin_ceil(__res * (long double)_M_max_load_factor);

      return __res;
    }

    // Return a bucket count appropriate for n elements
    std::size_t
    _M_bkt_for_elements(std::size_t __n) const noexcept
    { return __builtin_ceil(__n / (long double)_M_max_load_factor); }

    // __n_bkt is current bucket count, __n_elt is current element count,
    // and __n_ins is number of elements to be inserted.  Do we need to
    // increase bucket count?  If so, return make_pair(true, n), where n
    // is the new bucket count.  If not, return make_pair(false, 0).
    std::pair<bool, std::size_t>
    _M_need_rehash(std::size_t __n_bkt, std::size_t __n_elt,
		   std::size_t __n_ins) noexcept
    {
      if (__n_elt + __n_ins >= _M_next_resize)
	{
	  long double __min_bkts = (__n_elt + __n_ins)
					/ (long double)_M_max_load_factor;
	  if (__min_bkts >= __n_bkt)
	    return std::make_pair(true,
	      _M_next_bkt(std::max<std::size_t>(__builtin_floor(__min_bkts) + 1,
						__n_bkt * _S_growth_factor)));

	  _M_next_resize
	    = __builtin_floor(__n_bkt * (long double)_M_max_load_factor);
	  return std::make_pair(false, 0);
	}
      else
	return std::make_pair(false, 0);
    }

    typedef std::size_t _State;

    _State
    _M_state() const noexcept
    { return _M_next_resize; }

    void
    _M_reset() noexcept
    { _M_next_resize = 0; }

    void
    _M_reset(_State __state) noexcept
    { _M_next_resize = __state; }

    static const std::size_t _S_growth_factor = 2;

    float	_M_max_load_factor;
    std::size_t	_M_next_resize;
  };

  // Base classes for std::_Hashtable.  We define these base classes
  // because in some cases we want to do different things depending on
  // the value of a policy class.  In some cases the policy class
  // affects which member functions and nested typedefs are defined;
  // we handle that by specializing base class templates.  Several of
  // the base class templates need to access other members of class
  // template _Hashtable, so we use a variant of the "Curiously
  // Recurring Template Pattern" (CRTP) technique.

  /**
   *  Primary class template _Map_base.
   *
   *  If the hashtable has a value type of the form pair<T1, T2> and a
   *  key extraction policy (_ExtractKey) that returns the first part
   *  of the pair, the hashtable gets a mapped_type typedef.  If it
   *  satisfies those criteria and also has unique keys, then it also
   *  gets an operator[].
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   bool _Unique_keys = _Traits::__unique_keys::value>
    struct _Map_base { };

  /// Partial specialization, __unique_keys set to false.
  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, false>
    {
      using mapped_type = typename std::tuple_element<1, _Pair>::type;
    };

  /// Partial specialization, __unique_keys set to true.
  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
    {
    private:
      using __hashtable_base = __detail::_Hashtable_base<_Key, _Pair,
							 _Select1st,
							_Equal, _H1, _H2, _Hash,
							  _Traits>;

      using __hashtable = _Hashtable<_Key, _Pair, _Alloc,
				     _Select1st, _Equal,
				     _H1, _H2, _Hash, _RehashPolicy, _Traits>;

      using __hash_code = typename __hashtable_base::__hash_code;
      using __node_type = typename __hashtable_base::__node_type;

    public:
      using key_type = typename __hashtable_base::key_type;
      using iterator = typename __hashtable_base::iterator;
      using mapped_type = typename std::tuple_element<1, _Pair>::type;

      mapped_type&
      operator[](const key_type& __k);

      mapped_type&
      operator[](key_type&& __k);

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // DR 761. unordered_map needs an at() member function.
      mapped_type&
      at(const key_type& __k);

      const mapped_type&
      at(const key_type& __k) const;
    };

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    auto
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    operator[](const key_type& __k)                                      // ##flag #0
    -> mapped_type&
    {
      __hashtable* __h = static_cast<__hashtable*>(this); // trick. CRTP，在该文件顶部已经有__hashtable(_Hashtable<...>)的前向声明
      __hash_code __code = __h->_M_hash_code(__k);        // _M_hash_code也是属于_Hashtable的base class实现部分，突然发现这个CRTP trick有点好用了，不用管来自哪个base class
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p) // 如果没找到，那就需要插入了，否则直接return
	{
	  __p = __h->_M_allocate_node(std::piecewise_construct,
				      std::tuple<const key_type&>(__k),
				      std::tuple<>());
	  return __h->_M_insert_unique_node(__n, __code, __p)->second;
	}

      return __p->_M_v().second;
    }

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    auto
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    operator[](key_type&& __k)
    -> mapped_type&
    {
      __hashtable* __h = static_cast<__hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	{
	  __p = __h->_M_allocate_node(std::piecewise_construct,
				      std::forward_as_tuple(std::move(__k)),
				      std::tuple<>());
	  return __h->_M_insert_unique_node(__n, __code, __p)->second;
	}

      return __p->_M_v().second;
    }

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    auto
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    at(const key_type& __k)
    -> mapped_type&
    {
      __hashtable* __h = static_cast<__hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	__throw_out_of_range(__N("_Map_base::at"));
      return __p->_M_v().second;
    }

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    auto
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    at(const key_type& __k) const
    -> const mapped_type&
    {
      const __hashtable* __h = static_cast<const __hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	__throw_out_of_range(__N("_Map_base::at"));
      return __p->_M_v().second;
    }






























  /**
   *  Primary class template _Hashtable_ebo_helper.
   *
   *  Helper class using EBO when it is not forbidden (the type is not
   *  final) and when it is worth it (the type is empty.)
   */
  template<int _Nm, typename _Tp,
	   bool __use_ebo = !__is_final(_Tp) && __is_empty(_Tp)>
    struct _Hashtable_ebo_helper;

  /// Specialization using EBO.
  template<int _Nm, typename _Tp>
    struct _Hashtable_ebo_helper<_Nm, _Tp, true>
    : private _Tp
    {
      _Hashtable_ebo_helper() = default;

      template<typename _OtherTp>
	_Hashtable_ebo_helper(_OtherTp&& __tp)
	  : _Tp(std::forward<_OtherTp>(__tp))
	{ }

      static const _Tp&
      _S_cget(const _Hashtable_ebo_helper& __eboh)
      { return static_cast<const _Tp&>(__eboh); }

      static _Tp&
      _S_get(_Hashtable_ebo_helper& __eboh)
      { return static_cast<_Tp&>(__eboh); }
    };

  /// Specialization not using EBO.
  template<int _Nm, typename _Tp>
    struct _Hashtable_ebo_helper<_Nm, _Tp, false>
    {
      _Hashtable_ebo_helper() = default;

      template<typename _OtherTp>
	_Hashtable_ebo_helper(_OtherTp&& __tp)
	  : _M_tp(std::forward<_OtherTp>(__tp))
	{ }

      static const _Tp&
      _S_cget(const _Hashtable_ebo_helper& __eboh)
      { return __eboh._M_tp; }

      static _Tp&
      _S_get(_Hashtable_ebo_helper& __eboh)
      { return __eboh._M_tp; }

    private:
      _Tp _M_tp;
    };

  /**
   *  Primary class template _Hashtable_base.
   *
   *  Helper class adding management of _Equal functor to
   *  _Hash_code_base type.
   *
   *  Base class templates are:
   *    - __detail::_Hash_code_base
   *    - __detail::_Hashtable_ebo_helper
   */
  template<typename _Key, typename _Value,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash, typename _Traits>
  struct _Hashtable_base                                                           // ##flag #1
  : public _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2, _Hash,
			   _Traits::__hash_cached::value>,
    private _Hashtable_ebo_helper<0, _Equal>
  {
  public:
    typedef _Key					key_type;
    typedef _Value					value_type;
    typedef _Equal					key_equal;
    typedef std::size_t					size_type;
    typedef std::ptrdiff_t				difference_type;

    using __traits_type = _Traits;
    using __hash_cached = typename __traits_type::__hash_cached;
    using __constant_iterators = typename __traits_type::__constant_iterators;
    using __unique_keys = typename __traits_type::__unique_keys;

    using __hash_code_base = _Hash_code_base<_Key, _Value, _ExtractKey,
					     _H1, _H2, _Hash,
					     __hash_cached::value>;

    using __hash_code = typename __hash_code_base::__hash_code;
    using __node_type = typename __hash_code_base::__node_type;

    using iterator = __detail::_Node_iterator<value_type,
					      __constant_iterators::value,
					      __hash_cached::value>;

    using const_iterator = __detail::_Node_const_iterator<value_type,
						   __constant_iterators::value,
						   __hash_cached::value>;

    using local_iterator = __detail::_Local_iterator<key_type, value_type,
						  _ExtractKey, _H1, _H2, _Hash,
						  __constant_iterators::value,
						     __hash_cached::value>;

    using const_local_iterator = __detail::_Local_const_iterator<key_type,
								 value_type,
					_ExtractKey, _H1, _H2, _Hash,
					__constant_iterators::value,
					__hash_cached::value>;

    using __ireturn_type = typename std::conditional<__unique_keys::value,
						     std::pair<iterator, bool>,
						     iterator>::type;
  private:
    using _EqualEBO = _Hashtable_ebo_helper<0, _Equal>;
    using _EqualHelper =  _Equal_helper<_Key, _Value, _ExtractKey, _Equal,
					__hash_code, __hash_cached::value>;

  protected:
    _Hashtable_base() = default;
    _Hashtable_base(const _ExtractKey& __ex, const _H1& __h1, const _H2& __h2,
		    const _Hash& __hash, const _Equal& __eq)
    : __hash_code_base(__ex, __h1, __h2, __hash), _EqualEBO(__eq)
    { }

    bool
    _M_equals(const _Key& __k, __hash_code __c, __node_type* __n) const
    {
      static_assert(__is_invocable<const _Equal&, const _Key&, const _Key&>{},
	  "key equality predicate must be invocable with two arguments of "
	  "key type");
      return _EqualHelper::_S_equals(_M_eq(), this->_M_extract(),
				     __k, __c, __n);
    }

    void
    _M_swap(_Hashtable_base& __x)
    {
      __hash_code_base::_M_swap(__x);
      std::swap(_M_eq(), __x._M_eq());
    }

    const _Equal&
    _M_eq() const { return _EqualEBO::_S_cget(*this); }

    _Equal&
    _M_eq() { return _EqualEBO::_S_get(*this); }
  };

  /**
   *  struct _Equality_base.
   *
   *  Common types and functions for class _Equality.
   */
  struct _Equality_base
  {
  protected:
    template<typename _Uiterator>
      static bool
      _S_is_permutation(_Uiterator, _Uiterator, _Uiterator);
  };














 //@} hashtable-detail
} // namespace __detail
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std

#endif // _HASHTABLE_POLICY_H















namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

#include "../shared/hashtable-aux.cc"

namespace __detail
{
  // Return a prime no smaller than n.
  std::size_t
  _Prime_rehash_policy::_M_next_bkt(std::size_t __n) const
  {
    // Optimize lookups involving the first elements of __prime_list.
    // (useful to speed-up, eg, constructors)
    static const unsigned char __fast_bkt[] // trick. 无需lower_bound
      = { 2, 2, 2, 3, 5, 5, 7, 7, 11, 11, 11, 11, 13, 13 };

    if (__n < sizeof(__fast_bkt))
      {
	if (__n == 0)
	  // Special case on container 1st initialization with 0 bucket count
	  // hint. We keep _M_next_resize to 0 to make sure that next time we
	  // want to add an element allocation will take place.
	  return 1;

	_M_next_resize =
	  __builtin_floor(__fast_bkt[__n] * (double)_M_max_load_factor);
	return __fast_bkt[__n];
      }

    // Number of primes (without sentinel).
    constexpr auto __n_primes
      = sizeof(__prime_list) / sizeof(unsigned long) - 1;

    // Don't include the last prime in the search, so that anything
    // higher than the second-to-last prime returns a past-the-end
    // iterator that can be dereferenced to get the last prime.
    constexpr auto __last_prime = __prime_list + __n_primes - 1;

    const unsigned long* __next_bkt =
      std::lower_bound(__prime_list + 6, __last_prime, __n);

    if (__next_bkt == __last_prime)
      // Set next resize to the max value so that we never try to rehash again
      // as we already reach the biggest possible bucket number.
      // Note that it might result in max_load_factor not being respected.
      _M_next_resize = size_t(-1);
    else
      _M_next_resize =
	__builtin_floor(*__next_bkt * (double)_M_max_load_factor);

    return *__next_bkt;
  }

  // Finds the smallest prime p such that alpha p > __n_elt + __n_ins.
  // If p > __n_bkt, return make_pair(true, p); otherwise return
  // make_pair(false, 0).  In principle this isn't very different from
  // _M_bkt_for_elements.

  // The only tricky part is that we're caching the element count at
  // which we need to rehash, so we don't have to do a floating-point
  // multiply for every insertion.

  std::pair<bool, std::size_t>
  _Prime_rehash_policy::
  _M_need_rehash(std::size_t __n_bkt, std::size_t __n_elt,
		 std::size_t __n_ins) const
  {
    if (__n_elt + __n_ins > _M_next_resize)
      {
	// If _M_next_resize is 0 it means that we have nothing allocated so
	// far and that we start inserting elements. In this case we start
	// with an initial bucket size of 11.
	double __min_bkts
	  = std::max<std::size_t>(__n_elt + __n_ins, _M_next_resize ? 0 : 11)
	  / (double)_M_max_load_factor;
	if (__min_bkts >= __n_bkt)
	  return { true,
	    _M_next_bkt(std::max<std::size_t>(__builtin_floor(__min_bkts) + 1,
					      __n_bkt * _S_growth_factor)) };

	_M_next_resize
	  = __builtin_floor(__n_bkt * (double)_M_max_load_factor);
	return { false, 0 };
      }
    else
      return { false, 0 };
  }
} // namespace __detail

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std