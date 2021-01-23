// 如何在一个对象给出自身的shared_ptr/weak_ptr
// 如果直接返回shared_ptr<T>(this)，由于控制块是不同的，因此会产生错误的RAII
// 了解enable_shared_from_this如何处理这个问题
// EASTL也是直接参考boost的实现

// enable_shared_from_this只是简单持有一个空的weak_ptr
// 通过CRTP时的重载决议，在shared_ptr中赋值正确的控制块来实现


	template <typename T>
	class enable_shared_from_this
	{
	public:
		shared_ptr<T> shared_from_this()
			{ return shared_ptr<T>(mWeakPtr); }

		shared_ptr<const T> shared_from_this() const
			{ return shared_ptr<const T>(mWeakPtr); }

		weak_ptr<T> weak_from_this()
			{ return mWeakPtr; }

		weak_ptr<const T> weak_from_this() const
			{ return mWeakPtr; }

	public: // This is public because the alternative fails on some compilers that we need to support.
		mutable weak_ptr<T> mWeakPtr;

	protected:
		template <typename U> friend class shared_ptr;

		EA_CONSTEXPR enable_shared_from_this() EA_NOEXCEPT
			{ }

		enable_shared_from_this(const enable_shared_from_this&) EA_NOEXCEPT
			{ }

		enable_shared_from_this& operator=(const enable_shared_from_this&) EA_NOEXCEPT
			{ return *this; }

		~enable_shared_from_this()
			{ }

	}; // enable_shared_from_this


// class shard_ptr中

// 与enbable_shared_from_this相关的流程都集中在do_enable_shared_from_this，在构造时机调用

	/// do_enable_shared_from_this
	///
	/// If a user calls this function, it sets up mWeakPtr member of 
	/// the enable_shared_from_this parameter to point to the ref_count_sp 
	/// object that's passed in. Normally, the user doesn't need to call 
	/// this function, as the shared_ptr constructor will do it for them.
	///
	template <typename T, typename U>
	void do_enable_shared_from_this(const ref_count_sp* pRefCount,
	                                const enable_shared_from_this<T>* pEnableSharedFromThis,
	                                const U* pValue)
	{
		if (pEnableSharedFromThis)
			pEnableSharedFromThis->mWeakPtr.assign(const_cast<U*>(pValue), const_cast<ref_count_sp*>(pRefCount));
	}

	inline void do_enable_shared_from_this(const ref_count_sp*, ...) {} // Empty specialization. This no-op version is
	                                                                    // called by shared_ptr when shared_ptr's T type
	                                                                    // is anything but an enabled_shared_from_this
	                                                                    // class.

