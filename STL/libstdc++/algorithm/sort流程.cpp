// TL;DR 
// sort的具体实现为Introspective Sorting，并非纯粹的快排
// 源码中不仅能了解内省式排序的具体实现，还有一些技巧可借鉴，
// 包括但不限于玄学调参、if分支消去、递归和迭代混着使用的做法
// <del>另外可学到令人血压飙升的代码风格和注释了等于没注释的习惯</del>

// 代码展开是按照自顶向下的调用顺序
// 分析顺序不推荐按照这个流程走，已用STEP标记顺序


// STEP0和STEP1分析带有先判断first优化的插入排序流程
// STEP2和STEP3分析快速切分
// STEP4分析__introsort_loop的流程和它维护的性质
// STEP5和STEP6收尾
// 堆排部分省略

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

      std::__sort(__first, __last, __gnu_cxx::__ops::__iter_less_iter()); // comp()使用*it1 < *it2
    }

// STEP 6
  // sort真正的实现，可以称之为Introspective Sorting
  // 小于等于_S_threshold直接进行最朴素的插入排序（跳过__introsort_loop，只进入__final_insertion_sort不含优化的分支）
  template<typename _RandomAccessIterator, typename _Compare>
    inline void
    __sort(_RandomAccessIterator __first, _RandomAccessIterator __last,
	   _Compare __comp)
    {
      if (__first != __last)
	{
	  std::__introsort_loop(__first, __last,
        // https://stackoverflow.com/questions/40434664/what-is-std-lg
				std::__lg(__last - __first) * 2,   // 玄学参数，恕我没看懂这个取log实现，从结论上可认为是2*log_2(last-first)吧
				__comp);
    // __introsort_loop不一定对区间进行完全的排序，因此需要完整地用选择排序操作一遍，此前的区间已经大部分相对有序
	  std::__final_insertion_sort(__first, __last, __comp);
	}
    }

//////////////////////// 主要的两个步骤

// STEP 4
  /// This is a helper function for the sort routine.
  // 只有大于_S_threshold的个数才会执行
  // 如果递归深度未达到阈值（depth_limit）则会执行__unguarded_partition_pivot并两边递归处理__introsort_loop
  // 否则直接__partial_sort（堆排）
  // ================================     ##flag #3
  // 这里维护一个神奇的性质，就是最小值必然落于最左边的区间的前_S_threshold个元素中，
  // 只考虑最左边的区间的轴点pivot（或者叫cut），
  // 1. 如果在__depth_limit次数内就命中到[first + 0, first + _S_threshold)区间，最左边直接不再处理，且最小值在[first + 0, first + _S_threshold)内
  // 2. 如果在__depth_limit次数内某一次最左边的pivot仍未命中到[first + 0, first + _S_threshold)，则继续递归处理
  //     2.1. 如果__depth_limit-1 > 0，则继续求更左边的pivot
  //     2.2. 如果__depth_limit-1 == 0，则进入堆排流程(3)
  // 3. 如果已经处理到__depth_limit阈值外，堆排保证[first, 上一次的cut)的最小值就在first，并且整个子区间已排序，而因为该区间是轴点切分的，且是最左端的区间，因此最小值是整个区间（不只是子区间）的最小值
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
	  __last = __cut; // 用while来替代尾递归__introsort_loop(__first, __cut, __depth_limit, __comp)，但我寻思编译器会优化了吧
	}
    }

// STEP 5
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
    // 后面绝大多数元素都减少边界检查以提高性能
    // 这里直接unguarded是安全的，见##flag #3，可以说是整个sort流程最核心的技巧
    // 必须要求[first, first+_S_threshold)中肯定存在有比[first+_S_threshold, last)都更小的值
	  std::__unguarded_insertion_sort(__first + int(_S_threshold), __last,
					  __comp);
	}
      else
	std::__insertion_sort(__first, __last, __comp);
    }


//////////////////////// 具体实现展开

// TODO __introsort_loop部分展开（递归深度阈值内）


// STEP 2
  /// This is a helper function...
  template<typename _RandomAccessIterator, typename _Compare>
    inline _RandomAccessIterator
    __unguarded_partition_pivot(_RandomAccessIterator __first,
				_RandomAccessIterator __last, _Compare __comp)
    {
      // 轴点的选择是三点中值，first last和mid， Question.为啥是first+1，应该不必这样处理？
      _RandomAccessIterator __mid = __first + (__last - __first) / 2;
      /// Swaps the median value of *__a, *__b and *__c under __comp to *__result
      // 函数签名__move_median_to_first(_Iterator __result,_Iterator __a, _Iterator __b, _Iterator __c, _Compare __comp)
      // result是被swap，而不是直接移动覆盖掉原来的值
      std::__move_median_to_first(__first, __first + 1, __mid, __last - 1, // ##flag #2
				  __comp);
      return std::__unguarded_partition(__first + 1, __last, __first, __comp);
    }

// STEP 3
  /// This is a helper function...
  // 常规快排的选轴点过程，返回pivot所在位置，并且[first, pivot所在位置) (pivot, last)都处理好了
  // 没有iter_swap(_pivot, first)的过程，推测return的迭代器的值可能小于pivot？ 需要看下nth_element是否使用相同的做法(快速选择的实现是由pivot确认后修正first和last，然后小范围排序来确保nth的，与返回的pivot位置无关)
  // 按照##flag #2的做法，调用__unguarded_partition前从first到last应该是[=k][>k]...[any]...[<k]这样的区间，k是median值，any是原来的first值
  // （也可能是[=k][<k]...[any]...[>k]），不管怎样[first+1, last)不一定有严格为k的值，因此返回值只确保[__first, return_iter) 都<=k
  // case 0: [=<<<<<<<>>>>>>>] 此时return_iter为第一个>k
  // case 1: [=<<<<<<===>>>>>] 此时return_iter为中间的=
  // case 2: [=><>=>>>=<<>>><] 放过我吧
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
    // 此时first>=pivot
	  --__last;
	  while (__comp(__pivot, __last))
	    --__last;
    // 此时last<=pivot
    // first和last都是需要交换的非法的值
	  if (!(__first < __last)) // 如果first到达last或者交错的话则返回
	    return __first; // 如果first和last的值都为pivot，那么也会return，但不一定该迭代器的值会等于*pivot
	  std::iter_swap(__first, __last);
	  ++__first; // first和last充当lo和hi不断逼近的过程
	}
    }







// TODO __final_insertion_sort部分展开


// STEP 0
  /// This is a helper function for the sort routine.
  // 先判断first再进行常规插入排序
  template<typename _RandomAccessIterator, typename _Compare>
    void
    __insertion_sort(_RandomAccessIterator __first,
		     _RandomAccessIterator __last, _Compare __comp)
    {
      if (__first == __last) return;

      // [first, i)已处理
      for (_RandomAccessIterator __i = __first + 1; __i != __last; ++__i)
	{
	  if (__comp(__i, __first)) // first后某个i满足 *i < *first逆序     // ##flag #0
	    {
	      typename iterator_traits<_RandomAccessIterator>::value_type
		__val = _GLIBCXX_MOVE(*__i);
	      _GLIBCXX_MOVE_BACKWARD3(__first, __i, __i + 1); // moved [first, i) into i+1 区间右移
	      *__first = _GLIBCXX_MOVE(__val); // 上面空出一个位置，val插入到first
        // 一次性move过去，避免额外的逐个移动还要检查的次数（效率存疑）
	    }
	  else 
      // *i比*first大，但不保证(first, i)中与i没有逆序，因此需要常规地线性往前遍历，最多到first（肯定不符合##flag #1的条件）
      // 这样可以减少多次while时额外检查next==first的次数
	    std::__unguarded_linear_insert(__i,
				__gnu_cxx::__ops::__val_comp_iter(__comp));
	}
    }


// STEP 1
  /// This is a helper function for the sort routine.
  // 常规的插入排序流程
  template<typename _RandomAccessIterator, typename _Compare>
    void
    __unguarded_linear_insert(_RandomAccessIterator __last,
			      _Compare __comp)
    {
      typename iterator_traits<_RandomAccessIterator>::value_type
	__val = _GLIBCXX_MOVE(*__last);
      _RandomAccessIterator __next = __last;
      --__next; // next为前一个迭代器
      while (__comp(__val, __next))  // 仍然逆序 // ##flag #1
	{
    // 整个过程就是std::swap(*last--, *next--);
	  *__last = _GLIBCXX_MOVE(*__next);
	  __last = __next; // last往前走一步，缩小规模
	  --__next;
    // 在这个过程中，val是固定的值，而且是局部的变量，cache友好一点，不必同时取两个迭代器的值
	}
      *__last = _GLIBCXX_MOVE(__val); // sorted
    }


  /// This is a helper function for the sort routine.
  // 这个过程必须保证first之前（不含first）有一个更小的值存在
  template<typename _RandomAccessIterator, typename _Compare>
    inline void
    __unguarded_insertion_sort(_RandomAccessIterator __first,
			       _RandomAccessIterator __last, _Compare __comp)
    {
      for (_RandomAccessIterator __i = __first; __i != __last; ++__i)
	std::__unguarded_linear_insert(__i,
				__gnu_cxx::__ops::__val_comp_iter(__comp));
    }





// TODO 递归深度阈值外

  template<typename _RandomAccessIterator, typename _Compare>
    inline void
    __partial_sort(_RandomAccessIterator __first,
		   _RandomAccessIterator __middle,
		   _RandomAccessIterator __last,
		   _Compare __comp)
    {
      std::__heap_select(__first, __middle, __last, __comp);
      std::__sort_heap(__first, __middle, __comp);
    }

  /// This is a helper function for the sort routines.
  template<typename _RandomAccessIterator, typename _Compare>
    void
    __heap_select(_RandomAccessIterator __first,
		  _RandomAccessIterator __middle,
		  _RandomAccessIterator __last, _Compare __comp)
    {
      std::__make_heap(__first, __middle, __comp);
      for (_RandomAccessIterator __i = __middle; __i < __last; ++__i)
	if (__comp(__i, __first))
	  std::__pop_heap(__first, __middle, __i, __comp);
    }

  template<typename _RandomAccessIterator, typename _Compare>
    void
    __sort_heap(_RandomAccessIterator __first, _RandomAccessIterator __last,
		_Compare& __comp)
    {
      while (__last - __first > 1)
	{
	  --__last;
	  std::__pop_heap(__first, __last, __last, __comp);
	}
    }





// BONUS: 堆排更多细节

  template<typename _RandomAccessIterator, typename _Compare>
    void
    __make_heap(_RandomAccessIterator __first, _RandomAccessIterator __last,
		_Compare& __comp)
    {
      typedef typename iterator_traits<_RandomAccessIterator>::value_type
	  _ValueType;
      typedef typename iterator_traits<_RandomAccessIterator>::difference_type
	  _DistanceType;

      if (__last - __first < 2)
	return;

      const _DistanceType __len = __last - __first;
      _DistanceType __parent = (__len - 2) / 2;
      while (true)
	{
	  _ValueType __value = _GLIBCXX_MOVE(*(__first + __parent));
	  std::__adjust_heap(__first, __parent, __len, _GLIBCXX_MOVE(__value),
			     __comp);
	  if (__parent == 0)
	    return;
	  __parent--;
	}
    }



  template<typename _RandomAccessIterator, typename _Distance,
	   typename _Tp, typename _Compare>
    void
    __adjust_heap(_RandomAccessIterator __first, _Distance __holeIndex,
		  _Distance __len, _Tp __value, _Compare __comp)
    {
      const _Distance __topIndex = __holeIndex;
      _Distance __secondChild = __holeIndex;
      while (__secondChild < (__len - 1) / 2)
	{
	  __secondChild = 2 * (__secondChild + 1);
	  if (__comp(__first + __secondChild,
		     __first + (__secondChild - 1)))
	    __secondChild--;
	  *(__first + __holeIndex) = _GLIBCXX_MOVE(*(__first + __secondChild));
	  __holeIndex = __secondChild;
	}
      if ((__len & 1) == 0 && __secondChild == (__len - 2) / 2)
	{
	  __secondChild = 2 * (__secondChild + 1);
	  *(__first + __holeIndex) = _GLIBCXX_MOVE(*(__first
						     + (__secondChild - 1)));
	  __holeIndex = __secondChild - 1;
	}
      __decltype(__gnu_cxx::__ops::__iter_comp_val(_GLIBCXX_MOVE(__comp)))
	__cmp(_GLIBCXX_MOVE(__comp));
      std::__push_heap(__first, __holeIndex, __topIndex,
		       _GLIBCXX_MOVE(__value), __cmp);
    }



  // Heap-manipulation functions: push_heap, pop_heap, make_heap, sort_heap,
  // + is_heap and is_heap_until in C++0x.

  template<typename _RandomAccessIterator, typename _Distance, typename _Tp,
	   typename _Compare>
    void
    __push_heap(_RandomAccessIterator __first,
		_Distance __holeIndex, _Distance __topIndex, _Tp __value,
		_Compare& __comp)
    {
      _Distance __parent = (__holeIndex - 1) / 2;
      while (__holeIndex > __topIndex && __comp(__first + __parent, __value))
	{
	  *(__first + __holeIndex) = _GLIBCXX_MOVE(*(__first + __parent));
	  __holeIndex = __parent;
	  __parent = (__holeIndex - 1) / 2;
	}
      *(__first + __holeIndex) = _GLIBCXX_MOVE(__value);
    }



  template<typename _RandomAccessIterator, typename _Compare>
    inline void
    __pop_heap(_RandomAccessIterator __first, _RandomAccessIterator __last,
	       _RandomAccessIterator __result, _Compare& __comp)
    {
      typedef typename iterator_traits<_RandomAccessIterator>::value_type
	_ValueType;
      typedef typename iterator_traits<_RandomAccessIterator>::difference_type
	_DistanceType;

      _ValueType __value = _GLIBCXX_MOVE(*__result);
      *__result = _GLIBCXX_MOVE(*__first);
      std::__adjust_heap(__first, _DistanceType(0),
			 _DistanceType(__last - __first),
			 _GLIBCXX_MOVE(__value), __comp);
    }



// TODO __introselect只在nth_element中调用