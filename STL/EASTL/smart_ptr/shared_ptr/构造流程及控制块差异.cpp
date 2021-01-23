// 了解share_ptr构造与make_shared之间的差异

// shared_ptr主要处理两个成员
// T            *mpValue    // 被代理的指针
// ref_count_sp *mpRefCount // 控制块

// 关键在于控制块的实际类型不同
// 构造提供ref_count_sp_t
// make_shared提供ref_count_sp_t_inst

// 直接构造流程

        template <typename U>
		explicit shared_ptr(U* pValue,
		                    typename eastl::enable_if<eastl::is_convertible<U*, element_type*>::value>::type* = 0)
		    : mpValue(nullptr), mpRefCount(nullptr) // alloc_internal will set this.
		{
			// We explicitly use default_delete<U>. You can use the other version of this constructor to provide a
			// custom version.
			alloc_internal(pValue, default_allocator_type(),                                                                                            // TODO default_allocator_type
			               default_delete<U>()); // Problem: We want to be able to use default_deleter_type() instead of
			                                     // default_delete<U>, but if default_deleter_type's type is void or
			                                     // otherwise mismatched then this will fail to compile. What we really
			                                     // want to be able to do is "rebind" default_allocator_type to U
			                                     // instead of its original type.
		}


		// Handles the allocating of mpRefCount, while assigning mpValue.
		// The provided pValue may be NULL, as with constructing with a deleter and allocator but NULL pointer.
        // 在EASTL内部实现中，pValue必然是个指针
        // mpRefCount控制块类型为ref_count_sp_t，由allocator提供的内存进行placement new
        // 用type tag来处理enable_shared_from_this问题[略]
		template <typename U, typename Allocator, typename Deleter>
		void alloc_internal(U pValue, Allocator allocator, Deleter deleter)
		{
			typedef ref_count_sp_t<U, Allocator, Deleter> ref_count_type;

			#if EASTL_EXCEPTIONS_ENABLED
				try
				{
					void* const pMemory = EASTLAlloc(allocator, sizeof(ref_count_type));
					if(!pMemory) 
						throw std::bad_alloc();
					mpRefCount = ::new(pMemory) ref_count_type(pValue, eastl::move(deleter), eastl::move(allocator));
					mpValue = pValue;
					do_enable_shared_from_this(mpRefCount, pValue, pValue);
				}
				catch(...) // The exception would usually be std::bad_alloc.
				{
					deleter(pValue); // 20.7.2.2.1 p7: If an exception is thrown, delete p is called.
					throw;           // Throws: bad_alloc, or an implementation-defined exception when a resource other than memory could not be obtained.
				}
			#else
				void* const pMemory = EASTLAlloc(allocator, sizeof(ref_count_type));
				if(pMemory)
				{
					mpRefCount = ::new(pMemory) ref_count_type(pValue, eastl::move(deleter), eastl::move(allocator));
					mpValue = pValue;
					do_enable_shared_from_this(mpRefCount, pValue, pValue);
				}
				else
				{
					deleter(pValue);    // We act the same as we do above with exceptions enabled.
				}
			#endif
		}


// make_shared流程

	template <typename T, typename... Args>
	shared_ptr<T> make_shared(Args&&... args)
	{
		// allocate with the default allocator.
		return eastl::allocate_shared<T>(EASTL_SHARED_PTR_DEFAULT_ALLOCATOR, eastl::forward<Args>(args)...);                       // TODO EASTL_SHARED_PTR_DEFAULT_ALLOCATOR
	}

    // 与直接构造提供的控制块不同，提供的是ref_count_sp_t_inst类型
	template <typename T, typename Allocator, typename... Args>
	shared_ptr<T> allocate_shared(const Allocator& allocator, Args&&... args)
	{
		typedef ref_count_sp_t_inst<T, Allocator> ref_count_type;
		shared_ptr<T> ret;
		void* const pMemory = EASTLAlloc(const_cast<Allocator&>(allocator), sizeof(ref_count_type));
		if(pMemory)
		{
			ref_count_type* pRefCount = ::new(pMemory) ref_count_type(allocator, eastl::forward<Args>(args)...);
			allocate_shared_helper(ret, pRefCount, pRefCount->GetValue());
		}
		return ret;
	}

	template <typename T>
	void allocate_shared_helper(eastl::shared_ptr<T>& sharedPtr, ref_count_sp* pRefCount, T* pValue)
	{
		sharedPtr.mpRefCount = pRefCount;
		sharedPtr.mpValue = pValue;
		do_enable_shared_from_this(pRefCount, pValue, pValue);
	}

// 就是说要看控制块类型的区别

// ref_count_sp只提供引用计数
// ref_count_sp_t多提供了allocator和deleter，并且mValue假定是个指针类型
// ref_count_sp_t_inst多提供allocator，并且内部实际存有代理实例对象mMemory


// 构造函数是用户手动给出指针实例出的对象才能传入ref_count_sp_t
// 而make_shared的实例化对象是在ref_count_sp_t_inst存储的，因此整个控制块中，引用计数和类是一段连续的内存空间（基于继承）
// 但是make_shared并无法提供自定义析构器deleter，毕竟参数是直接完美转发过去的
// 行为的差异导致free相关的处理是有virtual开销的

	/// ref_count_sp
	///
	/// This is a small utility class used by shared_ptr and weak_ptr.
	struct ref_count_sp
	{
		int32_t mRefCount;            /// Reference count on the contained pointer. Starts as 1 by default.
		int32_t mWeakRefCount;        /// Reference count on contained pointer plus this ref_count_sp object itself. Starts as 1 by default.

	public:
		ref_count_sp(int32_t refCount = 1, int32_t weakRefCount = 1) EA_NOEXCEPT;
		virtual ~ref_count_sp() EA_NOEXCEPT {}

		int32_t       use_count() const EA_NOEXCEPT;
		void          addref() EA_NOEXCEPT;
		void          release();
		void          weak_addref() EA_NOEXCEPT;
		void          weak_release();
		ref_count_sp* lock() EA_NOEXCEPT;

		virtual void free_value() EA_NOEXCEPT = 0;          // Release the contained object.
		virtual void free_ref_count_sp() EA_NOEXCEPT = 0;   // Release this instance.

		#if EASTL_RTTI_ENABLED
			virtual void* get_deleter(const std::type_info& type) const EA_NOEXCEPT = 0;
		#else
			virtual void* get_deleter() const EA_NOEXCEPT = 0;
		#endif
	};


	/// ref_count_sp_t
	///
	/// This is a version of ref_count_sp which is used to delete the contained pointer.
	template <typename T, typename Allocator, typename Deleter>
	class ref_count_sp_t : public ref_count_sp
	{
	public:
		typedef ref_count_sp_t<T, Allocator, Deleter>   this_type;
		typedef T                                       value_type;
		typedef Allocator                               allocator_type;
		typedef Deleter                                 deleter_type;

		value_type     mValue; // This is expected to be a pointer.
		deleter_type   mDeleter;
		allocator_type mAllocator;

		ref_count_sp_t(value_type value, deleter_type deleter, allocator_type allocator)
			: ref_count_sp(), mValue(value), mDeleter(eastl::move(deleter)), mAllocator(eastl::move(allocator))
		{}

		void free_value() EA_NOEXCEPT
		{
			mDeleter(mValue);
			mValue = nullptr;
		}

		void free_ref_count_sp() EA_NOEXCEPT
		{
			allocator_type allocator = mAllocator;
			this->~ref_count_sp_t();
			EASTLFree(allocator, this, sizeof(*this));
		}

		#if EASTL_RTTI_ENABLED
			void* get_deleter(const std::type_info& type) const EA_NOEXCEPT
			{
				return (type == typeid(deleter_type)) ? (void*)&mDeleter : nullptr;
			}
		#else
			void* get_deleter() const EA_NOEXCEPT
			{
				return (void*)&mDeleter;
			}
		#endif
	};


	/// ref_count_sp_t_inst
	///
	/// This is a version of ref_count_sp which is used to actually hold an instance of
	/// T (instead of a pointer). This is useful to allocate the object and ref count
	/// in a single memory allocation.
	template<typename T, typename Allocator>
	class ref_count_sp_t_inst : public ref_count_sp
	{
	public:
		typedef ref_count_sp_t_inst<T, Allocator>                                        this_type;
		typedef T                                                                        value_type;
		typedef Allocator                                                                allocator_type;
		typedef typename aligned_storage<sizeof(T), eastl::alignment_of<T>::value>::type storage_type;

		storage_type   mMemory;
		allocator_type mAllocator;

		value_type* GetValue() { return static_cast<value_type*>(static_cast<void*>(&mMemory)); }

		template <typename... Args>
		ref_count_sp_t_inst(allocator_type allocator, Args&&... args)
			: ref_count_sp(), mAllocator(eastl::move(allocator))
		{
			new (&mMemory) value_type(eastl::forward<Args>(args)...);
		}

		void free_value() EA_NOEXCEPT
		{
			GetValue()->~value_type();
		}

		void free_ref_count_sp() EA_NOEXCEPT
		{
			allocator_type allocator = mAllocator;
			this->~ref_count_sp_t_inst();
			EASTLFree(allocator, this, sizeof(*this));
		}

		#if EASTL_RTTI_ENABLED
			void* get_deleter(const std::type_info&) const EA_NOEXCEPT
			{
				return nullptr; // Default base implementation.
			}
		#else
			void* get_deleter() const EA_NOEXCEPT
			{
				return nullptr;
			}
		#endif
	};