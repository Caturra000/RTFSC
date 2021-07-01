// 文件分布：basic_string.h

// 直接从std::hash<std::string>入手

  /// std::hash specialization for string.
  template<>
    struct hash<string>
    : public __hash_base<size_t, string> // 这个base似乎没啥用，libstdc++将它在C++17中移去了
    {
      size_t
      operator()(const string& __s) const noexcept
      { return std::_Hash_impl::hash(__s.data(), __s.length()); } // 实现见 ##flag #0
    };

  template<>
    struct __is_fast_hash<hash<string>> : std::false_type   // std::hash<std::string>并不是fast_hash
    { };














// 文件分布：functional_hash.h
// TL;DR hash内部实现接口 _Hash_impl / _Fnv_hash_impl
// 注意这是_Hash_impl，而不是直接的std::hash特化

  struct _Hash_impl
  {
    static size_t
    hash(const void* __ptr, size_t __clength,        // ##flag #0
	 size_t __seed = static_cast<size_t>(0xc70f6907UL))
    { return _Hash_bytes(__ptr, __clength, __seed); } // 接口位于hash_byte.h，实现在hash_byte.cc，简单的来说就是用murmur hash ##flag #1

    template<typename _Tp>
      static size_t
      hash(const _Tp& __val) // 对于没有特殊实现的类型，就是用sizeof来处理（注意这里是内部实现_Hash_impl，因此自定义类型还得要自己写std::hash，而不是标准库帮你搞定了）
      { return hash(&__val, sizeof(__val)); }

    template<typename _Tp>
      static size_t
      __hash_combine(const _Tp& __val, size_t __hash)
      { return hash(&__val, sizeof(__val), __hash); }
  };

  // A hash function similar to FNV-1a (see PR59406 for how it differs).
  // 并不打算看这个，常用的类型没有调用该实现
  struct _Fnv_hash_impl
  {
    static size_t
    hash(const void* __ptr, size_t __clength,
	 size_t __seed = static_cast<size_t>(2166136261UL))
    { return _Fnv_hash_bytes(__ptr, __clength, __seed); }

    template<typename _Tp>
      static size_t
      hash(const _Tp& __val)
      { return hash(&__val, sizeof(__val)); }

    template<typename _Tp>
      static size_t
      __hash_combine(const _Tp& __val, size_t __hash)
      { return hash(&__val, sizeof(__val), __hash); }
  };








  // 对于浮点也是用_Hash_impl的接口
  // 对于整型类见_Cxx_hashtable_define_trivial_hash接口

  /// Specialization for float.
  template<>
    struct hash<float> : public __hash_base<size_t, float>
    {
      size_t
      operator()(float __val) const noexcept
      {
	// 0 and -0 both hash to zero.
	return __val != 0.0f ? std::_Hash_impl::hash(__val) : 0;
      }
    };

  /// Specialization for double.
  template<>
    struct hash<double> : public __hash_base<size_t, double>
    {
      size_t
      operator()(double __val) const noexcept
      {
	// 0 and -0 both hash to zero.
	return __val != 0.0 ? std::_Hash_impl::hash(__val) : 0;
      }
    };

  /// Specialization for long double.
  template<>
    struct hash<long double>
    : public __hash_base<size_t, long double>
    {
      _GLIBCXX_PURE size_t
      operator()(long double __val) const noexcept;
    };

#if __cplusplus >= 201703L
  template<>
    struct hash<nullptr_t> : public __hash_base<size_t, nullptr_t>
    {
      size_t
      operator()(nullptr_t) const noexcept
      { return 0; }
    };
#endif

  // @} group hashes

  // Hint about performance of hash functor. If not fast the hash-based
  // containers will cache the hash code.
  // Default behavior is to consider that hashers are fast unless specified
  // otherwise.
  // 从注释可知，fast_hash用于给容器提供一个hint，来决定是否要cache hashcode，默认为true，当然也要看容器是否用了cache policy
  template<typename _Hash>
    struct __is_fast_hash : public std::true_type
    { };

  template<>
    struct __is_fast_hash<hash<long double>> : public std::false_type
    { };














// 文件分布：hash_bytes.cc
// TL;DR 一个murmur hash的实现
// _Hash_bytes实现分为3个版本，size_t大小为4字节/8字节/其它，这里只参考8字节版本
// 其它实现可看：https://github.com/gcc-mirror/gcc/blob/master/libstdc++-v3/libsupc++/hash_bytes.cc

  // Implementation of Murmur hash for 64-bit size_t.
  size_t
  _Hash_bytes(const void* ptr, size_t len, size_t seed) // ##flag #1
  {
    static const size_t mul = (((size_t) 0xc6a4a793UL) << 32UL)
			      + (size_t) 0x5bd1e995UL;
    const char* const buf = static_cast<const char*>(ptr);

    // Remove the bytes not divisible by the sizeof(size_t).  This
    // allows the main loop to process the data as 64-bit integers.
    const size_t len_aligned = len & ~(size_t)0x7;
    const char* const end = buf + len_aligned;
    size_t hash = seed ^ (len * mul);
    for (const char* p = buf; p != end; p += 8)
      {
	const size_t data = shift_mix(unaligned_load(p) * mul) * mul;
	hash ^= data;
	hash *= mul;
      }
    if ((len & 0x7) != 0)
      {
	const size_t data = load_bytes(end, len & 0x7);
	hash ^= data;
	hash *= mul;
      }
    hash = shift_mix(hash) * mul;
    hash = shift_mix(hash);
    return hash;
  }

  // Implementation of FNV hash for 64-bit size_t.
  // N.B. This function should work on unsigned char, otherwise it does not
  // correctly implement the FNV-1a algorithm (see PR59406).
  // The existing behaviour is retained for backwards compatibility.
  size_t
  _Fnv_hash_bytes(const void* ptr, size_t len, size_t hash)
  {
    const char* cptr = static_cast<const char*>(ptr);
    for (; len; --len)
      {
	hash ^= static_cast<size_t>(*cptr++);
	hash *= static_cast<size_t>(1099511628211ULL);
      }
    return hash;
  }



// 下面是一些helper function，包括shift_mix / unaligned_load / load_bytes

  inline std::size_t
  unaligned_load(const char* p)
  {
    std::size_t result;
    __builtin_memcpy(&result, p, sizeof(result));
    return result;
  }

  // Loads n bytes, where 1 <= n < 8.
  inline std::size_t
  load_bytes(const char* p, int n)
  {
    std::size_t result = 0;
    --n;
    do
      result = (result << 8) + static_cast<unsigned char>(p[n]);
    while (--n >= 0);
    return result;
  }

  inline std::size_t
  shift_mix(std::size_t v)
  { return v ^ (v >> 47);}
















// 文件分布：functional_hash.h
// 这部分处理基本类型，直接返回

#define _Cxx_hashtable_define_trivial_hash(_Tp) 	\
  template<>						\
    struct hash<_Tp> : public __hash_base<size_t, _Tp>  \
    {                                                   \
      size_t                                            \
      operator()(_Tp __val) const noexcept              \
      { return static_cast<size_t>(__val); }            \
    };

  /// Explicit specialization for bool.
  _Cxx_hashtable_define_trivial_hash(bool)

  /// Explicit specialization for char.
  _Cxx_hashtable_define_trivial_hash(char)

  /// Explicit specialization for signed char.
  _Cxx_hashtable_define_trivial_hash(signed char)

  /// Explicit specialization for unsigned char.
  _Cxx_hashtable_define_trivial_hash(unsigned char)

  /// Explicit specialization for wchar_t.
  _Cxx_hashtable_define_trivial_hash(wchar_t)



  /// Explicit specialization for int.
  _Cxx_hashtable_define_trivial_hash(int)

  /// Explicit specialization for long.
  _Cxx_hashtable_define_trivial_hash(long)

  /// Explicit specialization for long long.
  _Cxx_hashtable_define_trivial_hash(long long)

  /// Explicit specialization for unsigned short.
  _Cxx_hashtable_define_trivial_hash(unsigned short)

  /// Explicit specialization for unsigned int.
  _Cxx_hashtable_define_trivial_hash(unsigned int)

  /// Explicit specialization for unsigned long.
  _Cxx_hashtable_define_trivial_hash(unsigned long)

  /// Explicit specialization for unsigned long long.
  _Cxx_hashtable_define_trivial_hash(unsigned long long)