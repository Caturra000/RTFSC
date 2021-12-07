  /**
   *  @brief  Create an object that is owned by a shared_ptr.
   *  @param  __args  Arguments for the @a _Tp object's constructor.
   *  @return A shared_ptr that owns the newly created object.
   *  @throw  std::bad_alloc, or an exception thrown from the
   *          constructor of @a _Tp.
   */
  template<typename _Tp, typename... _Args>
    inline shared_ptr<_Tp>
    make_shared(_Args&&... __args)
    {
      typedef typename std::remove_cv<_Tp>::type _Tp_nc;
      return std::allocate_shared<_Tp>(std::allocator<_Tp_nc>(),
				       std::forward<_Args>(__args)...);
    }

  /**
   *  @brief  Create an object that is owned by a shared_ptr.
   *  @param  __a     An allocator.
   *  @param  __args  Arguments for the @a _Tp object's constructor.
   *  @return A shared_ptr that owns the newly created object.
   *  @throw  An exception thrown from @a _Alloc::allocate or from the
   *          constructor of @a _Tp.
   *
   *  A copy of @a __a will be used to allocate memory for the shared_ptr
   *  and the new object.
   */
  template<typename _Tp, typename _Alloc, typename... _Args>
    inline shared_ptr<_Tp>
    allocate_shared(const _Alloc& __a, _Args&&... __args)
    {
      return shared_ptr<_Tp>(_Sp_alloc_shared_tag<_Alloc>{__a},
			     std::forward<_Args>(__args)...);
    }








  template<typename _Tp, _Lock_policy _Lp = __default_lock_policy,
	   typename... _Args>
    inline __shared_ptr<_Tp, _Lp>
    __make_shared(_Args&&... __args)
    {
      typedef typename std::remove_const<_Tp>::type _Tp_nc;
      return std::__allocate_shared<_Tp, _Lp>(std::allocator<_Tp_nc>(),
					      std::forward<_Args>(__args)...);
    }

  template<typename _Tp, _Lock_policy _Lp = __default_lock_policy,
	   typename _Alloc, typename... _Args>
    inline __shared_ptr<_Tp, _Lp>
    __allocate_shared(const _Alloc& __a, _Args&&... __args)
    {
      return __shared_ptr<_Tp, _Lp>(_Sp_alloc_shared_tag<_Alloc>{__a},
				    std::forward<_Args>(__args)...);
    }