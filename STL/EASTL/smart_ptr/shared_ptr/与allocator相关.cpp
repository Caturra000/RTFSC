EASTL_SHARED_PTR_DEFAULT_ALLOCATOR EASTLAllocatorType(EASTL_SHARED_PTR_DEFAULT_NAME)



	template <typename T, typename... Args>
	shared_ptr<T> make_shared(Args&&... args)
	{
		// allocate with the default allocator.
		return eastl::allocate_shared<T>(EASTL_SHARED_PTR_DEFAULT_ALLOCATOR, eastl::forward<Args>(args)...);
	}

	// allocate_shared传入EASTLAllocatorType(EASTL_SHARED_PTR_DEFAULT_NAME)作为allocator



		template <typename U>
		explicit shared_ptr(U* pValue,
		                    typename eastl::enable_if<eastl::is_convertible<U*, element_type*>::value>::type* = 0)
		    : mpValue(nullptr), mpRefCount(nullptr) // alloc_internal will set this.
		{
			// We explicitly use default_delete<U>. You can use the other version of this constructor to provide a
			// custom version.
			alloc_internal(pValue, default_allocator_type(),
			               default_delete<U>()); // Problem: We want to be able to use default_deleter_type() instead of
			                                     // default_delete<U>, but if default_deleter_type's type is void or
			                                     // otherwise mismatched then this will fail to compile. What we really
			                                     // want to be able to do is "rebind" default_allocator_type to U
			                                     // instead of its original type.
		}

		// alloc_internal内部 void* const pMemory = EASTLAlloc(allocator, sizeof(ref_count_type));



		typedef EASTLAllocatorType                               default_allocator_type;


// config.h START

#ifndef EASTLAllocatorType
	#define EASTLAllocatorType eastl::allocator
#endif

// config.h END

