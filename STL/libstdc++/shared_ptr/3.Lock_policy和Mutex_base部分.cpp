// 引用计数本身是线程安全的
// 而libstdc++允许不同的并发策略
// 默认使用_S_atomic
// 如果不希望有线程安全的额外成本，可以指定policy为_S_single
//
// 具体使用于shared_count部分(_Sp_counted_base)
//
// 我觉得_Lock_policy一个不太好的地方是
// 对于shared_ptr中不涉及线程安全的操作，实现上也会依赖上这个template enum
// 比如shared_ptr_access<Tp, Lp>，对于pointer操作来说，根本就跟线程安全无关

  // Available locking policies:
  // _S_single    single-threaded code that doesn't need to be locked.
  // _S_mutex     multi-threaded code that requires additional support
  //              from gthr.h or abstraction layers in concurrence.h.
  // _S_atomic    multi-threaded code using atomic operations.
  enum _Lock_policy { _S_single, _S_mutex, _S_atomic }; 

  // Compile time constant that indicates prefered locking policy in
  // the current configuration.
  static const _Lock_policy __default_lock_policy = 
#ifndef __GTHREADS
  _S_single;
#elif defined _GLIBCXX_HAVE_ATOMIC_LOCK_POLICY
  _S_atomic;
#else
  _S_mutex;
#endif






  // Empty helper class except when the template argument is _S_mutex.
  template<_Lock_policy _Lp>
    class _Mutex_base
    {
    protected:
      // The atomic policy uses fully-fenced builtins, single doesn't care.
      enum { _S_need_barriers = 0 };
    };

  template<>
    class _Mutex_base<_S_mutex>
    : public __gnu_cxx::__mutex
    {
    protected:
      // This policy is used when atomic builtins are not available.
      // The replacement atomic operations might not have the necessary
      // memory barriers.
      enum { _S_need_barriers = 1 };
    };
