  // helpers for make_shared / allocate_shared

  struct _Sp_make_shared_tag
  {
  private:
    template<typename _Tp, typename _Alloc, _Lock_policy _Lp>
      friend class _Sp_counted_ptr_inplace;

    static const type_info&
    _S_ti() noexcept _GLIBCXX_VISIBILITY(default)
    {
      alignas(type_info) static constexpr char __tag[sizeof(type_info)] = { };
      return reinterpret_cast<const type_info&>(__tag);
    }

    static bool _S_eq(const type_info&) noexcept;
  };

  template<typename _Alloc>
    struct _Sp_alloc_shared_tag
    {
      const _Alloc& _M_a;
    };
