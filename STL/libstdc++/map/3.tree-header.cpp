// header维护的结点挺有意思
// _M_node_count就是结点个数了
// _M_header._M_parent == root
// _M_header._M_left == leftmost
// _M_header._M_right == rightmost
// 这些可以在class _Rb_tree的函数 _M_root() _M_leftmost() _M_rightmost() 找出来

  // Helper type to manage default initialization of node count and header.
  struct _Rb_tree_header
  {
    _Rb_tree_node_base	_M_header;
    size_t		_M_node_count; // Keeps track of size of tree.

    _Rb_tree_header() _GLIBCXX_NOEXCEPT
    {
      _M_header._M_color = _S_red;
      _M_reset();
    }

#if __cplusplus >= 201103L
    _Rb_tree_header(_Rb_tree_header&& __x) noexcept
    {
      if (__x._M_header._M_parent != nullptr)
	_M_move_data(__x);
      else
	{
	  _M_header._M_color = _S_red;
	  _M_reset();
	}
    }
#endif

    void
    _M_move_data(_Rb_tree_header& __from)
    {
      _M_header._M_color = __from._M_header._M_color;
      _M_header._M_parent = __from._M_header._M_parent;
      _M_header._M_left = __from._M_header._M_left;
      _M_header._M_right = __from._M_header._M_right;
      _M_header._M_parent->_M_parent = &_M_header;
      _M_node_count = __from._M_node_count;

      __from._M_reset();
    }

    void
    _M_reset()
    {
      _M_header._M_parent = 0;
      _M_header._M_left = &_M_header;
      _M_header._M_right = &_M_header;
      _M_node_count = 0;
    }
  };