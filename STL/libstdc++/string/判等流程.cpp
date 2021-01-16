// TL;DR
// 并没有任何技巧上的优化
//
// 大概流程
// 1. operator==就是直接调用compare
// 2. basic_string委托给char_traits
// 3. 就是逐个字符的比较



  // operator ==
  /**
   *  @brief  Test equivalence of two strings.
   *  @param __lhs  First string.
   *  @param __rhs  Second string.
   *  @return  True if @a __lhs.compare(@a __rhs) == 0.  False otherwise.
   */
  template <typename _CharT, typename _Traits, typename _Alloc>
  inline bool operator==(const basic_string<_CharT, _Traits, _Alloc>& __lhs,
                         const basic_string<_CharT, _Traits, _Alloc>& __rhs)
      _GLIBCXX_NOEXCEPT {
    return __lhs.compare(__rhs) == 0;
  }

    /**
     *  @brief  Compare to a string.
     *  @param __str  String to compare against.
     *  @return  Integer < 0, 0, or > 0.
     *
     *  Returns an integer < 0 if this string is ordered before @a
     *  __str, 0 if their values are equivalent, or > 0 if this
     *  string is ordered after @a __str.  Determines the effective
     *  length rlen of the strings to compare as the smallest of
     *  size() and str.size().  The function then compares the two
     *  strings by calling traits::compare(data(), str.data(),rlen).
     *  If the result of the comparison is nonzero returns it,
     *  otherwise the shorter one is ordered first.
     */
    // 委托到traits_type来完成
    int compare(const basic_string& __str) const {
      const size_type __size = this->size();
      const size_type __osize = __str.size();
      const size_type __len = std::min(__size, __osize);

      int __r = traits_type::compare(_M_data(), __str.data(), __len);
      if (!__r)
        __r = _S_compare(__size, __osize);
      return __r;
    }


  template<typename _CharT>
    _GLIBCXX14_CONSTEXPR int
    char_traits<_CharT>::
    compare(const char_type* __s1, const char_type* __s2, std::size_t __n)
    {
      for (std::size_t __i = 0; __i < __n; ++__i)
	if (lt(__s1[__i], __s2[__i]))
	  return -1;
	else if (lt(__s2[__i], __s1[__i]))
	  return 1;
      return 0;
    }