// _M_key_compare并没有用EBO优化，估计是考虑到比较器是有状态的

  // Helper type offering value initialization guarantee on the compare functor.
  template<typename _Key_compare>
    struct _Rb_tree_key_compare
    {
      _Key_compare		_M_key_compare;

      _Rb_tree_key_compare()
      _GLIBCXX_NOEXCEPT_IF(
	is_nothrow_default_constructible<_Key_compare>::value)
      : _M_key_compare()
      { }

      _Rb_tree_key_compare(const _Key_compare& __comp)
      : _M_key_compare(__comp)
      { }

#if __cplusplus >= 201103L
      // Copy constructor added for consistency with C++98 mode.
      _Rb_tree_key_compare(const _Rb_tree_key_compare&) = default;

      _Rb_tree_key_compare(_Rb_tree_key_compare&& __x)
	noexcept(is_nothrow_copy_constructible<_Key_compare>::value)
      : _M_key_compare(__x._M_key_compare)
      { }
#endif
    };