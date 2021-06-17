// 节点部分分开为两个类的实现，同样有base class继承操作

  enum _Rb_tree_color { _S_red = false, _S_black = true };

  struct _Rb_tree_node_base
  {
    typedef _Rb_tree_node_base* _Base_ptr;
    typedef const _Rb_tree_node_base* _Const_Base_ptr;

    _Rb_tree_color	_M_color;
    _Base_ptr		_M_parent;
    _Base_ptr		_M_left;
    _Base_ptr		_M_right;

    static _Base_ptr
    _S_minimum(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_left != 0) __x = __x->_M_left;
      return __x;
    }

    static _Const_Base_ptr
    _S_minimum(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_left != 0) __x = __x->_M_left;
      return __x;
    }

    static _Base_ptr
    _S_maximum(_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_right != 0) __x = __x->_M_right;
      return __x;
    }

    static _Const_Base_ptr
    _S_maximum(_Const_Base_ptr __x) _GLIBCXX_NOEXCEPT
    {
      while (__x->_M_right != 0) __x = __x->_M_right;
      return __x;
    }
  };







  template<typename _Val>
    struct _Rb_tree_node : public _Rb_tree_node_base
    {
      typedef _Rb_tree_node<_Val>* _Link_type;

#if __cplusplus < 201103L
      _Val _M_value_field;

      _Val*
      _M_valptr()
      { return std::__addressof(_M_value_field); }

      const _Val*
      _M_valptr() const
      { return std::__addressof(_M_value_field); }
#else
      __gnu_cxx::__aligned_membuf<_Val> _M_storage;

      _Val*
      _M_valptr()
      { return _M_storage._M_ptr(); }

      const _Val*
      _M_valptr() const
      { return _M_storage._M_ptr(); }
#endif
    };

  _GLIBCXX_PURE _Rb_tree_node_base*
  _Rb_tree_increment(_Rb_tree_node_base* __x) throw ();

  _GLIBCXX_PURE const _Rb_tree_node_base*
  _Rb_tree_increment(const _Rb_tree_node_base* __x) throw ();

  _GLIBCXX_PURE _Rb_tree_node_base*
  _Rb_tree_decrement(_Rb_tree_node_base* __x) throw ();

  _GLIBCXX_PURE const _Rb_tree_node_base*
  _Rb_tree_decrement(const _Rb_tree_node_base* __x) throw ();



  void
  _Rb_tree_insert_and_rebalance(const bool __insert_left,
				_Rb_tree_node_base* __x,
				_Rb_tree_node_base* __p,
				_Rb_tree_node_base& __header) throw ();

  _Rb_tree_node_base*
  _Rb_tree_rebalance_for_erase(_Rb_tree_node_base* const __z,
			       _Rb_tree_node_base& __header) throw ();