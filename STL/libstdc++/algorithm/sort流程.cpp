  /**
   *  @brief Sort the elements of a sequence.
   *  @ingroup sorting_algorithms
   *  @param  __first   An iterator.
   *  @param  __last    Another iterator.
   *  @return  Nothing.
   *
   *  Sorts the elements in the range @p [__first,__last) in ascending order,
   *  such that for each iterator @e i in the range @p [__first,__last-1),  
   *  *(i+1)<*i is false.
   *
   *  The relative ordering of equivalent elements is not preserved, use
   *  @p stable_sort() if this is needed.
  */
  template<typename _RandomAccessIterator>
    inline void
    sort(_RandomAccessIterator __first, _RandomAccessIterator __last)
    {
      // concept requirements
      __glibcxx_function_requires(_Mutable_RandomAccessIteratorConcept<
	    _RandomAccessIterator>)
      __glibcxx_function_requires(_LessThanComparableConcept<
	    typename iterator_traits<_RandomAccessIterator>::value_type>)
      __glibcxx_requires_valid_range(__first, __last);
      __glibcxx_requires_irreflexive(__first, __last);

      std::__sort(__first, __last, __gnu_cxx::__ops::__iter_less_iter());
    }


  template<typename _RandomAccessIterator, typename _Compare>
    inline void
    __sort(_RandomAccessIterator __first, _RandomAccessIterator __last,
	   _Compare __comp)
    {
      if (__first != __last)
	{
	  std::__introsort_loop(__first, __last,
				std::__lg(__last - __first) * 2,   // 玄学参数
				__comp);
	  std::__final_insertion_sort(__first, __last, __comp);
	}
    }


  /// This is a helper function for the sort routine.
  // 只有大于_S_threshold的个数才会执行
  // 如果递归深度未达到阈值（depth_limit）则会执行__unguarded_partition_pivot并两边递归处理__introsort_loop
  // 否则直接__partial_sort
  template<typename _RandomAccessIterator, typename _Size, typename _Compare>
    void
    __introsort_loop(_RandomAccessIterator __first,
		     _RandomAccessIterator __last,
		     _Size __depth_limit, _Compare __comp)
    {
      while (__last - __first > int(_S_threshold)) // 超过_S_threshold个元素才会执行整个流程
	{
	  if (__depth_limit == 0)
	    {
	      std::__partial_sort(__first, __last, __last, __comp);
	      return;
	    }
	  --__depth_limit;
	  _RandomAccessIterator __cut =
	    std::__unguarded_partition_pivot(__first, __last, __comp);
	  std::__introsort_loop(__cut, __last, __depth_limit, __comp);
	  __last = __cut;
	}
    }

  /// This is a helper function for the sort routine.
  // 大于_S_threshold对前_S_threshold元素进行insertion_sort，剩余__unguarded_insertion_sort
  template<typename _RandomAccessIterator, typename _Compare>
    void
    __final_insertion_sort(_RandomAccessIterator __first,
			   _RandomAccessIterator __last, _Compare __comp)
    {
      if (__last - __first > int(_S_threshold))
	{
	  std::__insertion_sort(__first, __first + int(_S_threshold), __comp);
	  std::__unguarded_insertion_sort(__first + int(_S_threshold), __last,
					  __comp);
	}
      else
	std::__insertion_sort(__first, __last, __comp);
    }




// TODO __introsort_loop部分展开



  /// This is a helper function...
  template<typename _RandomAccessIterator, typename _Compare>
    inline _RandomAccessIterator
    __unguarded_partition_pivot(_RandomAccessIterator __first,
				_RandomAccessIterator __last, _Compare __comp)
    {
      _RandomAccessIterator __mid = __first + (__last - __first) / 2;
      /// Swaps the median value of *__a, *__b and *__c under __comp to *__result
      /// 函数签名__move_median_to_first(_Iterator __result,_Iterator __a, _Iterator __b, _Iterator __c, _Compare __comp)
      std::__move_median_to_first(__first, __first + 1, __mid, __last - 1,
				  __comp);
      return std::__unguarded_partition(__first + 1, __last, __first, __comp);
    }


  /// This is a helper function...
  template<typename _RandomAccessIterator, typename _Compare>
    _RandomAccessIterator
    __unguarded_partition(_RandomAccessIterator __first,
			  _RandomAccessIterator __last,
			  _RandomAccessIterator __pivot, _Compare __comp)
    {
      while (true)
	{
	  while (__comp(__first, __pivot))
	    ++__first;
	  --__last;
	  while (__comp(__pivot, __last))
	    --__last;
	  if (!(__first < __last))
	    return __first;
	  std::iter_swap(__first, __last);
	  ++__first;
	}
    }







// TODO __final_insertion_sort部分展开

  /// This is a helper function for the sort routine.
  template<typename _RandomAccessIterator, typename _Compare>
    void
    __insertion_sort(_RandomAccessIterator __first,
		     _RandomAccessIterator __last, _Compare __comp)
    {
      if (__first == __last) return;

      for (_RandomAccessIterator __i = __first + 1; __i != __last; ++__i)
	{
	  if (__comp(__i, __first))
	    {
	      typename iterator_traits<_RandomAccessIterator>::value_type
		__val = _GLIBCXX_MOVE(*__i);
	      _GLIBCXX_MOVE_BACKWARD3(__first, __i, __i + 1);
	      *__first = _GLIBCXX_MOVE(__val);
	    }
	  else
	    std::__unguarded_linear_insert(__i,
				__gnu_cxx::__ops::__val_comp_iter(__comp));
	}
    }

  /// This is a helper function for the sort routine.
  template<typename _RandomAccessIterator, typename _Compare>
    void
    __unguarded_linear_insert(_RandomAccessIterator __last,
			      _Compare __comp)
    {
      typename iterator_traits<_RandomAccessIterator>::value_type
	__val = _GLIBCXX_MOVE(*__last);
      _RandomAccessIterator __next = __last;
      --__next;
      while (__comp(__val, __next))
	{
	  *__last = _GLIBCXX_MOVE(*__next);
	  __last = __next;
	  --__next;
	}
      *__last = _GLIBCXX_MOVE(__val);
    }


  /// This is a helper function for the sort routine.
  template<typename _RandomAccessIterator, typename _Compare>
    inline void
    __unguarded_insertion_sort(_RandomAccessIterator __first,
			       _RandomAccessIterator __last, _Compare __comp)
    {
      for (_RandomAccessIterator __i = __first; __i != __last; ++__i)
	std::__unguarded_linear_insert(__i,
				__gnu_cxx::__ops::__val_comp_iter(__comp));
    }


// TODO __introselect只在nth_element中调用