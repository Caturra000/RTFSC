// 文件：bionic/linker/linker.cpp
bool soinfo::prelink_image() {
  if (flags_ & FLAG_PRELINKED) return true;
  /* Extract dynamic section */
  ElfW(Word) dynamic_flags = 0;
  phdr_table_get_dynamic_section(phdr, phnum, load_bias, &dynamic, &dynamic_flags);

  /* We can't log anything until the linker is relocated */
  bool relocating_linker = (flags_ & FLAG_LINKER) != 0;
  if (!relocating_linker) {
    INFO("[ Linking \"%s\" ]", get_realpath());
    DEBUG("si->base = %p si->flags = 0x%08x", reinterpret_cast<void*>(base), flags_);
  }

  if (dynamic == nullptr) {
    if (!relocating_linker) {
      DL_ERR("missing PT_DYNAMIC in \"%s\"", get_realpath());
    }
    return false;
  } else {
    if (!relocating_linker) {
      DEBUG("dynamic = %p", dynamic);
    }
  }

#if defined(__arm__)
  (void) phdr_table_get_arm_exidx(phdr, phnum, load_bias,
                                  &ARM_exidx, &ARM_exidx_count);
#endif

  TlsSegment tls_segment;
  if (__bionic_get_tls_segment(phdr, phnum, load_bias, &tls_segment)) {
    if (!__bionic_check_tls_alignment(&tls_segment.alignment)) {
      if (!relocating_linker) {
        DL_ERR("TLS segment alignment in \"%s\" is not a power of 2: %zu",
               get_realpath(), tls_segment.alignment);
      }
      return false;
    }
    tls_ = std::make_unique<soinfo_tls>();
    tls_->segment = tls_segment;
  }

  // Extract useful information from dynamic section.
  // Note that: "Except for the DT_NULL element at the end of the array,
  // and the relative order of DT_NEEDED elements, entries may appear in any order."
  //
  // source: http://www.sco.com/developers/gabi/1998-04-29/ch5.dynamic.html
  uint32_t needed_count = 0;
  for (ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
    DEBUG("d = %p, d[0](tag) = %p d[1](val) = %p",
          d, reinterpret_cast<void*>(d->d_tag), reinterpret_cast<void*>(d->d_un.d_val));
    switch (d->d_tag) {
      case DT_SONAME:
        // this is parsed after we have strtab initialized (see below).
        break;

      case DT_HASH:
        nbucket_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[0];
        nchain_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[1];
        bucket_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr + 8);
        chain_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr + 8 + nbucket_ * 4);
        break;

      case DT_GNU_HASH:
        gnu_nbucket_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[0];
        // skip symndx
        gnu_maskwords_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[2];
        gnu_shift2_ = reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[3];

        gnu_bloom_filter_ = reinterpret_cast<ElfW(Addr)*>(load_bias + d->d_un.d_ptr + 16);
        gnu_bucket_ = reinterpret_cast<uint32_t*>(gnu_bloom_filter_ + gnu_maskwords_);
        // amend chain for symndx = header[1]
        gnu_chain_ = gnu_bucket_ + gnu_nbucket_ -
            reinterpret_cast<uint32_t*>(load_bias + d->d_un.d_ptr)[1];

        if (!powerof2(gnu_maskwords_)) {
          DL_ERR("invalid maskwords for gnu_hash = 0x%x, in \"%s\" expecting power to two",
              gnu_maskwords_, get_realpath());
          return false;
        }
        --gnu_maskwords_;

        flags_ |= FLAG_GNU_HASH;
        break;

      case DT_STRTAB:
        strtab_ = reinterpret_cast<const char*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_STRSZ:
        strtab_size_ = d->d_un.d_val;
        break;

      case DT_SYMTAB:
        symtab_ = reinterpret_cast<ElfW(Sym)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_SYMENT:
        if (d->d_un.d_val != sizeof(ElfW(Sym))) {
          DL_ERR("invalid DT_SYMENT: %zd in \"%s\"",
              static_cast<size_t>(d->d_un.d_val), get_realpath());
          return false;
        }
        break;

      case DT_PLTREL:
#if defined(USE_RELA)
        if (d->d_un.d_val != DT_RELA) {
          DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_RELA", get_realpath());
          return false;
        }
#else
        if (d->d_un.d_val != DT_REL) {
          DL_ERR("unsupported DT_PLTREL in \"%s\"; expected DT_REL", get_realpath());
          return false;
        }
#endif
        break;

      case DT_JMPREL:
#if defined(USE_RELA)
        plt_rela_ = reinterpret_cast<ElfW(Rela)*>(load_bias + d->d_un.d_ptr);
#else
        plt_rel_ = reinterpret_cast<ElfW(Rel)*>(load_bias + d->d_un.d_ptr);
#endif
        break;

      case DT_PLTRELSZ:
#if defined(USE_RELA)
        plt_rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
#else
        plt_rel_count_ = d->d_un.d_val / sizeof(ElfW(Rel));
#endif
        break;

      case DT_PLTGOT:
        // Ignored (because RTLD_LAZY is not supported).
        break;

      case DT_DEBUG:
        // Set the DT_DEBUG entry to the address of _r_debug for GDB
        // if the dynamic table is writable
        if ((dynamic_flags & PF_W) != 0) {
          d->d_un.d_val = reinterpret_cast<uintptr_t>(&_r_debug);
        }
        break;
#if defined(USE_RELA)
      case DT_RELA:
        rela_ = reinterpret_cast<ElfW(Rela)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_RELASZ:
        rela_count_ = d->d_un.d_val / sizeof(ElfW(Rela));
        break;

      case DT_ANDROID_RELA:
        android_relocs_ = reinterpret_cast<uint8_t*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_ANDROID_RELASZ:
        android_relocs_size_ = d->d_un.d_val;
        break;

      case DT_ANDROID_REL:
        DL_ERR("unsupported DT_ANDROID_REL in \"%s\"", get_realpath());
        return false;

      case DT_ANDROID_RELSZ:
        DL_ERR("unsupported DT_ANDROID_RELSZ in \"%s\"", get_realpath());
        return false;

      case DT_RELAENT:
        if (d->d_un.d_val != sizeof(ElfW(Rela))) {
          DL_ERR("invalid DT_RELAENT: %zd", static_cast<size_t>(d->d_un.d_val));
          return false;
        }
        break;

      // Ignored (see DT_RELCOUNT comments for details).
      case DT_RELACOUNT:
        break;

      case DT_REL:
        DL_ERR("unsupported DT_REL in \"%s\"", get_realpath());
        return false;

      case DT_RELSZ:
        DL_ERR("unsupported DT_RELSZ in \"%s\"", get_realpath());
        return false;

#else
      case DT_REL:
        rel_ = reinterpret_cast<ElfW(Rel)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_RELSZ:
        rel_count_ = d->d_un.d_val / sizeof(ElfW(Rel));
        break;

      case DT_RELENT:
        if (d->d_un.d_val != sizeof(ElfW(Rel))) {
          DL_ERR("invalid DT_RELENT: %zd", static_cast<size_t>(d->d_un.d_val));
          return false;
        }
        break;

      case DT_ANDROID_REL:
        android_relocs_ = reinterpret_cast<uint8_t*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_ANDROID_RELSZ:
        android_relocs_size_ = d->d_un.d_val;
        break;

      case DT_ANDROID_RELA:
        DL_ERR("unsupported DT_ANDROID_RELA in \"%s\"", get_realpath());
        return false;

      case DT_ANDROID_RELASZ:
        DL_ERR("unsupported DT_ANDROID_RELASZ in \"%s\"", get_realpath());
        return false;

      // "Indicates that all RELATIVE relocations have been concatenated together,
      // and specifies the RELATIVE relocation count."
      //
      // TODO: Spec also mentions that this can be used to optimize relocation process;
      // Not currently used by bionic linker - ignored.
      case DT_RELCOUNT:
        break;

      case DT_RELA:
        DL_ERR("unsupported DT_RELA in \"%s\"", get_realpath());
        return false;

      case DT_RELASZ:
        DL_ERR("unsupported DT_RELASZ in \"%s\"", get_realpath());
        return false;

#endif
      case DT_RELR:
      case DT_ANDROID_RELR:
        relr_ = reinterpret_cast<ElfW(Relr)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_RELRSZ:
      case DT_ANDROID_RELRSZ:
        relr_count_ = d->d_un.d_val / sizeof(ElfW(Relr));
        break;

      case DT_RELRENT:
      case DT_ANDROID_RELRENT:
        if (d->d_un.d_val != sizeof(ElfW(Relr))) {
          DL_ERR("invalid DT_RELRENT: %zd", static_cast<size_t>(d->d_un.d_val));
          return false;
        }
        break;

      // Ignored (see DT_RELCOUNT comments for details).
      // There is no DT_RELRCOUNT specifically because it would only be ignored.
      case DT_ANDROID_RELRCOUNT:
        break;

      case DT_INIT:
        init_func_ = reinterpret_cast<linker_ctor_function_t>(load_bias + d->d_un.d_ptr);
        DEBUG("%s constructors (DT_INIT) found at %p", get_realpath(), init_func_);
        break;

      case DT_FINI:
        fini_func_ = reinterpret_cast<linker_dtor_function_t>(load_bias + d->d_un.d_ptr);
        DEBUG("%s destructors (DT_FINI) found at %p", get_realpath(), fini_func_);
        break;

      case DT_INIT_ARRAY:
        init_array_ = reinterpret_cast<linker_ctor_function_t*>(load_bias + d->d_un.d_ptr);
        DEBUG("%s constructors (DT_INIT_ARRAY) found at %p", get_realpath(), init_array_);
        break;

      case DT_INIT_ARRAYSZ:
        init_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
        break;

      case DT_FINI_ARRAY:
        fini_array_ = reinterpret_cast<linker_dtor_function_t*>(load_bias + d->d_un.d_ptr);
        DEBUG("%s destructors (DT_FINI_ARRAY) found at %p", get_realpath(), fini_array_);
        break;

      case DT_FINI_ARRAYSZ:
        fini_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
        break;

      case DT_PREINIT_ARRAY:
        preinit_array_ = reinterpret_cast<linker_ctor_function_t*>(load_bias + d->d_un.d_ptr);
        DEBUG("%s constructors (DT_PREINIT_ARRAY) found at %p", get_realpath(), preinit_array_);
        break;

      case DT_PREINIT_ARRAYSZ:
        preinit_array_count_ = static_cast<uint32_t>(d->d_un.d_val) / sizeof(ElfW(Addr));
        break;

      case DT_TEXTREL:
#if defined(__LP64__)
        DL_ERR("\"%s\" has text relocations", get_realpath());
        return false;
#else
        has_text_relocations = true;
        break;
#endif

      case DT_SYMBOLIC:
        has_DT_SYMBOLIC = true;
        break;

      case DT_NEEDED:
        ++needed_count;
        break;

      case DT_FLAGS:
        if (d->d_un.d_val & DF_TEXTREL) {
#if defined(__LP64__)
          DL_ERR("\"%s\" has text relocations", get_realpath());
          return false;
#else
          has_text_relocations = true;
#endif
        }
        if (d->d_un.d_val & DF_SYMBOLIC) {
          has_DT_SYMBOLIC = true;
        }
        break;

      case DT_FLAGS_1:
        set_dt_flags_1(d->d_un.d_val);

        if ((d->d_un.d_val & ~SUPPORTED_DT_FLAGS_1) != 0) {
          DL_WARN("Warning: \"%s\" has unsupported flags DT_FLAGS_1=%p "
                  "(ignoring unsupported flags)",
                  get_realpath(), reinterpret_cast<void*>(d->d_un.d_val));
        }
        break;

      // Ignored: "Its use has been superseded by the DF_BIND_NOW flag"
      case DT_BIND_NOW:
        break;

      case DT_VERSYM:
        versym_ = reinterpret_cast<ElfW(Versym)*>(load_bias + d->d_un.d_ptr);
        break;

      case DT_VERDEF:
        verdef_ptr_ = load_bias + d->d_un.d_ptr;
        break;
      case DT_VERDEFNUM:
        verdef_cnt_ = d->d_un.d_val;
        break;

      case DT_VERNEED:
        verneed_ptr_ = load_bias + d->d_un.d_ptr;
        break;

      case DT_VERNEEDNUM:
        verneed_cnt_ = d->d_un.d_val;
        break;

      case DT_RUNPATH:
        // this is parsed after we have strtab initialized (see below).
        break;

      case DT_TLSDESC_GOT:
      case DT_TLSDESC_PLT:
        // These DT entries are used for lazy TLSDESC relocations. Bionic
        // resolves everything eagerly, so these can be ignored.
        break;

#if defined(__aarch64__)
      case DT_AARCH64_BTI_PLT:
      case DT_AARCH64_PAC_PLT:
      case DT_AARCH64_VARIANT_PCS:
        // Ignored: AArch64 processor-specific dynamic array tags.
        break;
#endif

      default:
        if (!relocating_linker) {
          const char* tag_name;
          if (d->d_tag == DT_RPATH) {
            tag_name = "DT_RPATH";
          } else if (d->d_tag == DT_ENCODING) {
            tag_name = "DT_ENCODING";
          } else if (d->d_tag >= DT_LOOS && d->d_tag <= DT_HIOS) {
            tag_name = "unknown OS-specific";
          } else if (d->d_tag >= DT_LOPROC && d->d_tag <= DT_HIPROC) {
            tag_name = "unknown processor-specific";
          } else {
            tag_name = "unknown";
          }
          DL_WARN("Warning: \"%s\" unused DT entry: %s (type %p arg %p) (ignoring)",
                  get_realpath(),
                  tag_name,
                  reinterpret_cast<void*>(d->d_tag),
                  reinterpret_cast<void*>(d->d_un.d_val));
        }
        break;
    }
  }

  DEBUG("si->base = %p, si->strtab = %p, si->symtab = %p",
        reinterpret_cast<void*>(base), strtab_, symtab_);

  // Validity checks.
  if (relocating_linker && needed_count != 0) {
    DL_ERR("linker cannot have DT_NEEDED dependencies on other libraries");
    return false;
  }
  if (nbucket_ == 0 && gnu_nbucket_ == 0) {
    DL_ERR("empty/missing DT_HASH/DT_GNU_HASH in \"%s\" "
        "(new hash type from the future?)", get_realpath());
    return false;
  }
  if (strtab_ == nullptr) {
    DL_ERR("empty/missing DT_STRTAB in \"%s\"", get_realpath());
    return false;
  }
  if (symtab_ == nullptr) {
    DL_ERR("empty/missing DT_SYMTAB in \"%s\"", get_realpath());
    return false;
  }

  // Second pass - parse entries relying on strtab. Skip this while relocating the linker so as to
  // avoid doing heap allocations until later in the linker's initialization.
  if (!relocating_linker) {
    for (ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d) {
      switch (d->d_tag) {
        case DT_SONAME:
          set_soname(get_string(d->d_un.d_val));
          break;
        case DT_RUNPATH:
          set_dt_runpath(get_string(d->d_un.d_val));
          break;
      }
    }
  }

  // Before M release, linker was using basename in place of soname. In the case when DT_SONAME is
  // absent some apps stop working because they can't find DT_NEEDED library by soname. This
  // workaround should keep them working. (Applies only for apps targeting sdk version < M.) Make
  // an exception for the main executable, which does not need to have DT_SONAME. The linker has an
  // DT_SONAME but the soname_ field is initialized later on.
  if (soname_.empty() && this != solist_get_somain() && !relocating_linker &&
      get_application_target_sdk_version() < 23) {
    soname_ = basename(realpath_.c_str());
    DL_WARN_documented_change(23, "missing-soname-enforced-for-api-level-23",
                              "\"%s\" has no DT_SONAME (will use %s instead)", get_realpath(),
                              soname_.c_str());

    // Don't call add_dlwarning because a missing DT_SONAME isn't important enough to show in the UI
  }

  // Validate each library's verdef section once, so we don't have to validate
  // it each time we look up a symbol with a version.
  if (!validate_verdef_section(this)) return false;

  flags_ |= FLAG_PRELINKED;
  return true;
}
