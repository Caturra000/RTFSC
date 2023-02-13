// 文件：bionic/linker/linker.cpp
bool soinfo::link_image(const SymbolLookupList& lookup_list, soinfo* local_group_root,
                        const android_dlextinfo* extinfo, size_t* relro_fd_offset) {
  if (is_image_linked()) {
    // already linked.
    return true;
  }

  if (g_is_ldd && !is_main_executable()) {
    async_safe_format_fd(STDOUT_FILENO, "\t%s => %s (%p)\n", get_soname(),
                         get_realpath(), reinterpret_cast<void*>(base));
  }

  local_group_root_ = local_group_root;
  if (local_group_root_ == nullptr) {
    local_group_root_ = this;
  }

  if ((flags_ & FLAG_LINKER) == 0 && local_group_root_ == this) {
    target_sdk_version_ = get_application_target_sdk_version();
  }

#if !defined(__LP64__)
  if (has_text_relocations) {
    // Fail if app is targeting M or above.
    int app_target_api_level = get_application_target_sdk_version();
    if (app_target_api_level >= 23) {
      DL_ERR_AND_LOG("\"%s\" has text relocations (%s#Text-Relocations-Enforced-for-API-level-23)",
                     get_realpath(), kBionicChangesUrl);
      return false;
    }
    // Make segments writable to allow text relocations to work properly. We will later call
    // phdr_table_protect_segments() after all of them are applied.
    DL_WARN_documented_change(23,
                              "Text-Relocations-Enforced-for-API-level-23",
                              "\"%s\" has text relocations",
                              get_realpath());
    add_dlwarning(get_realpath(), "text relocations");
    if (phdr_table_unprotect_segments(phdr, phnum, load_bias) < 0) {
      DL_ERR("can't unprotect loadable segments for \"%s\": %s", get_realpath(), strerror(errno));
      return false;
    }
  }
#endif

  if (!relocate(lookup_list)) {
    return false;
  }

  DEBUG("[ finished linking %s ]", get_realpath());

#if !defined(__LP64__)
  if (has_text_relocations) {
    // All relocations are done, we can protect our segments back to read-only.
    if (phdr_table_protect_segments(phdr, phnum, load_bias) < 0) {
      DL_ERR("can't protect segments for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  }
#endif

  // We can also turn on GNU RELRO protection if we're not linking the dynamic linker
  // itself --- it can't make system calls yet, and will have to call protect_relro later.
  if (!is_linker() && !protect_relro()) {
    return false;
  }

  /* Handle serializing/sharing the RELRO segment */
  if (extinfo && (extinfo->flags & ANDROID_DLEXT_WRITE_RELRO)) {
    if (phdr_table_serialize_gnu_relro(phdr, phnum, load_bias,
                                       extinfo->relro_fd, relro_fd_offset) < 0) {
      DL_ERR("failed serializing GNU RELRO section for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  } else if (extinfo && (extinfo->flags & ANDROID_DLEXT_USE_RELRO)) {
    if (phdr_table_map_gnu_relro(phdr, phnum, load_bias,
                                 extinfo->relro_fd, relro_fd_offset) < 0) {
      DL_ERR("failed mapping GNU RELRO section for \"%s\": %s",
             get_realpath(), strerror(errno));
      return false;
    }
  }

  ++g_module_load_counter;
  notify_gdb_of_load(this);
  // 标记完成
  set_image_linked();
  return true;
}

// 文件：bionic/linker/linker_relocate.cpp

bool soinfo::relocate(const SymbolLookupList& lookup_list) {

  VersionTracker version_tracker;

  if (!version_tracker.init(this)) {
    return false;
  }

  // 参考：https://cs.android.com/android/platform/superproject/+/android-13.0.0_r18:bionic/linker/linker_relocate.cpp;l=59
  Relocator relocator(version_tracker, lookup_list);
  relocator.si = this;
  relocator.si_strtab = strtab_;
  relocator.si_strtab_size = has_min_version(1) ? strtab_size_ : SIZE_MAX;
  relocator.si_symtab = symtab_;
  relocator.tlsdesc_args = &tlsdesc_args_;
  relocator.tls_tp_base = __libc_shared_globals()->static_tls_layout.offset_thread_pointer();

  // 如果存在DT_ANDROID_RELA
  // 参考：https://reviews.llvm.org/D39152
  if (android_relocs_ != nullptr) {
    // check signature
    if (android_relocs_size_ > 3 &&
        android_relocs_[0] == 'A' &&
        android_relocs_[1] == 'P' &&
        android_relocs_[2] == 'S' &&
        android_relocs_[3] == '2') {
      DEBUG("[ android relocating %s ]", get_realpath());

      const uint8_t* packed_relocs = android_relocs_ + 4;
      const size_t packed_relocs_size = android_relocs_size_ - 4;

      if (!packed_relocate<RelocMode::Typical>(relocator, sleb128_decoder(packed_relocs, packed_relocs_size))) {
        return false;
      }
    } else {
      DL_ERR("bad android relocation header.");
      return false;
    }
  }

  // DT_RELR或者DT_ANDROID_RELR
  if (relr_ != nullptr) {
    DEBUG("[ relocating %s relr ]", get_realpath());
    if (!relocate_relr()) {
      return false;
    }
  }

// android在LP64下使用的是RELA
// 参考：https://cs.android.com/android/platform/superproject/+/android-13.0.0_r18:bionic/linker/linker_common_types.h;l=38
#if defined(USE_RELA)
  if (rela_ != nullptr) {
    DEBUG("[ relocating %s rela ]", get_realpath());

    if (!plain_relocate<RelocMode::Typical>(relocator, rela_, rela_count_)) {
      return false;
    }
  }
  if (plt_rela_ != nullptr) {
    DEBUG("[ relocating %s plt rela ]", get_realpath());
    if (!plain_relocate<RelocMode::JumpTable>(relocator, plt_rela_, plt_rela_count_)) {
      return false;
    }
  }
#else
  // 对应DT_REL
  if (rel_ != nullptr) {
    DEBUG("[ relocating %s rel ]", get_realpath());
    if (!plain_relocate<RelocMode::Typical>(relocator, rel_, rel_count_)) {
      return false;
    }
  }
  // 对应DT_JMPREL
  if (plt_rel_ != nullptr) {
    DEBUG("[ relocating %s plt rel ]", get_realpath());
    if (!plain_relocate<RelocMode::JumpTable>(relocator, plt_rel_, plt_rel_count_)) {
      return false;
    }
  }
#endif

  // Once the tlsdesc_args_ vector's size is finalized, we can write the addresses of its elements
  // into the TLSDESC relocations.
#if defined(__aarch64__)
  // Bionic currently only implements TLSDESC for arm64.
  for (const std::pair<TlsDescriptor*, size_t>& pair : relocator.deferred_tlsdesc_relocs) {
    TlsDescriptor* desc = pair.first;
    desc->func = tlsdesc_resolver_dynamic;
    desc->arg = reinterpret_cast<size_t>(&tlsdesc_args_[pair.second]);
  }
#endif

  return true;
}

// packed_relocate...

template <RelocMode OptMode, typename ...Args>
static bool packed_relocate(Relocator& relocator, Args ...args) {
  // 如果开启DEBUG，那么使用slow relocate loop
  return needs_slow_relocate_loop(relocator) ?
      packed_relocate_impl<RelocMode::General>(relocator, args...) :
      packed_relocate_impl<OptMode>(relocator, args...);
}

template <RelocMode Mode>
__attribute__((noinline))
static bool packed_relocate_impl(Relocator& relocator, sleb128_decoder decoder) {
  return for_all_packed_relocs(decoder, [&](const rel_t& reloc) {
    return process_relocation<Mode>(relocator, reloc);
  });
}

// plain_relocate...

template <RelocMode OptMode, typename ...Args>
static bool plain_relocate(Relocator& relocator, Args ...args) {
  return needs_slow_relocate_loop(relocator) ?
      plain_relocate_impl<RelocMode::General>(relocator, args...) :
      plain_relocate_impl<OptMode>(relocator, args...);
}

template <RelocMode Mode>
__attribute__((noinline))
static bool plain_relocate_impl(Relocator& relocator, rel_t* rels, size_t rel_count) {
  for (size_t i = 0; i < rel_count; ++i) {
    if (!process_relocation<Mode>(relocator, rels[i])) {
      return false;
    }
  }
  return true;
}

template <RelocMode Mode>
__attribute__((always_inline))
static inline bool process_relocation(Relocator& relocator, const rel_t& reloc) {
  return Mode == RelocMode::General ?
      process_relocation_general(relocator, reloc) :
      process_relocation_impl<Mode>(relocator, reloc);
}

__attribute__((noinline))
static bool process_relocation_general(Relocator& relocator, const rel_t& reloc) {
  return process_relocation_impl<RelocMode::General>(relocator, reloc);
}

// 可以先跟踪Mode == RelocMode::Typical
template <RelocMode Mode>
__attribute__((always_inline))
static bool process_relocation_impl(Relocator& relocator, const rel_t& reloc) {
  constexpr bool IsGeneral = Mode == RelocMode::General;

  void* const rel_target = reinterpret_cast<void*>(reloc.r_offset + relocator.si->load_bias);
  const uint32_t r_type = ELFW(R_TYPE)(reloc.r_info);
  const uint32_t r_sym = ELFW(R_SYM)(reloc.r_info);

  soinfo* found_in = nullptr;
  const ElfW(Sym)* sym = nullptr;
  const char* sym_name = nullptr;
  ElfW(Addr) sym_addr = 0;

  if (r_sym != 0) {
    sym_name = relocator.get_string(relocator.si_symtab[r_sym].st_name);
  }

  // While relocating a DSO with text relocations (obsolete and 32-bit only), the .text segment is
  // writable (but not executable). To call an ifunc, temporarily remap the segment as executable
  // (but not writable). Then switch it back to continue applying relocations in the segment.
#if defined(__LP64__)
  const bool handle_text_relocs = false;
  auto protect_segments = []() { return true; };
  auto unprotect_segments = []() { return true; };
#else
  const bool handle_text_relocs = IsGeneral && relocator.si->has_text_relocations;
  auto protect_segments = [&]() {
    // Make .text executable.
    if (phdr_table_protect_segments(relocator.si->phdr, relocator.si->phnum,
                                    relocator.si->load_bias) < 0) {
      DL_ERR("can't protect segments for \"%s\": %s",
             relocator.si->get_realpath(), strerror(errno));
      return false;
    }
    return true;
  };
  auto unprotect_segments = [&]() {
    // Make .text writable.
    if (phdr_table_unprotect_segments(relocator.si->phdr, relocator.si->phnum,
                                      relocator.si->load_bias) < 0) {
      DL_ERR("can't unprotect loadable segments for \"%s\": %s",
             relocator.si->get_realpath(), strerror(errno));
      return false;
    }
    return true;
  };
#endif

  auto trace_reloc = [](const char* fmt, ...) __printflike(2, 3) {
    if (IsGeneral &&
        g_ld_debug_verbosity > LINKER_VERBOSITY_TRACE &&
        DO_TRACE_RELO) {
      va_list ap;
      va_start(ap, fmt);
      linker_log_va_list(LINKER_VERBOSITY_TRACE, fmt, ap);
      va_end(ap);
    }
  };

  // Skip symbol lookup for R_GENERIC_NONE relocations.
  if (__predict_false(r_type == R_GENERIC_NONE)) {
    trace_reloc("RELO NONE");
    return true;
  }

#if defined(USE_RELA)
  auto get_addend_rel   = [&]() -> ElfW(Addr) { return reloc.r_addend; };
  auto get_addend_norel = [&]() -> ElfW(Addr) { return reloc.r_addend; };
#else
  auto get_addend_rel   = [&]() -> ElfW(Addr) { return *static_cast<ElfW(Addr)*>(rel_target); };
  auto get_addend_norel = [&]() -> ElfW(Addr) { return 0; };
#endif

  if (!IsGeneral && __predict_false(is_tls_reloc(r_type))) {
    // Always process TLS relocations using the slow code path, so that STB_LOCAL symbols are
    // diagnosed, and ifunc processing is skipped.
    return process_relocation_general(relocator, reloc);
  }

  if (IsGeneral && is_tls_reloc(r_type)) {
    if (r_sym == 0) {
      // By convention in ld.bfd and lld, an omitted symbol on a TLS relocation
      // is a reference to the current module.
      found_in = relocator.si;
    } else if (ELF_ST_BIND(relocator.si_symtab[r_sym].st_info) == STB_LOCAL) {
      // In certain situations, the Gold linker accesses a TLS symbol using a
      // relocation to an STB_LOCAL symbol in .dynsym of either STT_SECTION or
      // STT_TLS type. Bionic doesn't support these relocations, so issue an
      // error. References:
      //  - https://groups.google.com/d/topic/generic-abi/dJ4_Y78aQ2M/discussion
      //  - https://sourceware.org/bugzilla/show_bug.cgi?id=17699
      sym = &relocator.si_symtab[r_sym];
      auto sym_type = ELF_ST_TYPE(sym->st_info);
      if (sym_type == STT_SECTION) {
        DL_ERR("unexpected TLS reference to local section in \"%s\": sym type %d, rel type %u",
               relocator.si->get_realpath(), sym_type, r_type);
      } else {
        DL_ERR(
            "unexpected TLS reference to local symbol \"%s\" in \"%s\": sym type %d, rel type %u",
            sym_name, relocator.si->get_realpath(), sym_type, r_type);
      }
      return false;
    } else if (!lookup_symbol<IsGeneral>(relocator, r_sym, sym_name, &found_in, &sym)) {
      return false;
    }
    if (found_in != nullptr && found_in->get_tls() == nullptr) {
      // sym_name can be nullptr if r_sym is 0. A linker should never output an ELF file like this.
      DL_ERR("TLS relocation refers to symbol \"%s\" in solib \"%s\" with no TLS segment",
             sym_name, found_in->get_realpath());
      return false;
    }
    if (sym != nullptr) {
      if (ELF_ST_TYPE(sym->st_info) != STT_TLS) {
        // A toolchain should never output a relocation like this.
        DL_ERR("reference to non-TLS symbol \"%s\" from TLS relocation in \"%s\"",
               sym_name, relocator.si->get_realpath());
        return false;
      }
      sym_addr = sym->st_value;
    }
  } else {
    if (r_sym == 0) {
      // Do nothing.
    } else {
      // 找出sym（过程麻烦略了），relocator会对找到的结果进行cache处理
      if (!lookup_symbol<IsGeneral>(relocator, r_sym, sym_name, &found_in, &sym)) return false;
      if (sym != nullptr) {
        const bool should_protect_segments = handle_text_relocs &&
                                             found_in == relocator.si &&
                                             ELF_ST_TYPE(sym->st_info) == STT_GNU_IFUNC;
        if (should_protect_segments && !protect_segments()) return false;
        sym_addr = found_in->resolve_symbol_address(sym);
        if (should_protect_segments && !unprotect_segments()) return false;
      } else if constexpr (IsGeneral) {
        // A weak reference to an undefined symbol. We typically use a zero symbol address, but
        // use the relocation base for PC-relative relocations, so that the value written is zero.
        switch (r_type) {
#if defined(__x86_64__)
          case R_X86_64_PC32:
            sym_addr = reinterpret_cast<ElfW(Addr)>(rel_target);
            break;
#elif defined(__i386__)
          case R_386_PC32:
            sym_addr = reinterpret_cast<ElfW(Addr)>(rel_target);
            break;
#endif
        }
      }
    }
  }

  if constexpr (IsGeneral || Mode == RelocMode::JumpTable) {
    if (r_type == R_GENERIC_JUMP_SLOT) {
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      const ElfW(Addr) result = sym_addr + get_addend_norel();
      trace_reloc("RELO JMP_SLOT %16p <- %16p %s",
                  rel_target, reinterpret_cast<void*>(result), sym_name);
      *static_cast<ElfW(Addr)*>(rel_target) = result;
      return true;
    }
  }

  // 跟踪这里
  if constexpr (IsGeneral || Mode == RelocMode::Typical) {
    // Almost all dynamic relocations are of one of these types, and most will be
    // R_GENERIC_ABSOLUTE. The platform typically uses RELR instead, but R_GENERIC_RELATIVE is
    // common in non-platform binaries.
    // 一般来说r_type都是这个，谷歌在这里做了个跨平台类型R_GENERIC_ABSOLUTE
    // 见：https://cs.android.com/android/platform/superproject/+/master:bionic/linker/linker_relocs.h;l=93
    if (r_type == R_GENERIC_ABSOLUTE) {
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      const ElfW(Addr) result = sym_addr + get_addend_rel();
      trace_reloc("RELO ABSOLUTE %16p <- %16p %s",
                  rel_target, reinterpret_cast<void*>(result), sym_name);
      // 实际上的重定位操作
      *static_cast<ElfW(Addr)*>(rel_target) = result;
      return true;
    } else if (r_type == R_GENERIC_GLOB_DAT) {
      // The i386 psABI specifies that R_386_GLOB_DAT doesn't have an addend. The ARM ELF ABI
      // document (IHI0044F) specifies that R_ARM_GLOB_DAT has an addend, but Bionic isn't adding
      // it.
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      const ElfW(Addr) result = sym_addr + get_addend_norel();
      trace_reloc("RELO GLOB_DAT %16p <- %16p %s",
                  rel_target, reinterpret_cast<void*>(result), sym_name);
      *static_cast<ElfW(Addr)*>(rel_target) = result;
      return true;
    } else if (r_type == R_GENERIC_RELATIVE) {
      // In practice, r_sym is always zero, but if it weren't, the linker would still look up the
      // referenced symbol (and abort if the symbol isn't found), even though it isn't used.
      count_relocation_if<IsGeneral>(kRelocRelative);
      const ElfW(Addr) result = relocator.si->load_bias + get_addend_rel();
      trace_reloc("RELO RELATIVE %16p <- %16p",
                  rel_target, reinterpret_cast<void*>(result));
      *static_cast<ElfW(Addr)*>(rel_target) = result;
      return true;
    }
  }

  // 见下方注释，在这之前基本都处理好了
  if constexpr (!IsGeneral) {
    // Almost all relocations are handled above. Handle the remaining relocations below, in a
    // separate function call. The symbol lookup will be repeated, but the result should be served
    // from the 1-symbol lookup cache.
    return process_relocation_general(relocator, reloc);
  }

  switch (r_type) {
    case R_GENERIC_IRELATIVE:
      // In the linker, ifuncs are called as soon as possible so that string functions work. We must
      // not call them again. (e.g. On arm32, resolving an ifunc changes the meaning of the addend
      // from a resolver function to the implementation.)
      if (!relocator.si->is_linker()) {
        count_relocation_if<IsGeneral>(kRelocRelative);
        const ElfW(Addr) ifunc_addr = relocator.si->load_bias + get_addend_rel();
        trace_reloc("RELO IRELATIVE %16p <- %16p",
                    rel_target, reinterpret_cast<void*>(ifunc_addr));
        if (handle_text_relocs && !protect_segments()) return false;
        const ElfW(Addr) result = call_ifunc_resolver(ifunc_addr);
        if (handle_text_relocs && !unprotect_segments()) return false;
        *static_cast<ElfW(Addr)*>(rel_target) = result;
      }
      break;
    case R_GENERIC_COPY:
      // Copy relocations allow read-only data or code in a non-PIE executable to access a
      // variable from a DSO. The executable reserves extra space in its .bss section, and the
      // linker copies the variable into the extra space. The executable then exports its copy
      // to interpose the copy in the DSO.
      //
      // Bionic only supports PIE executables, so copy relocations aren't supported. The ARM and
      // AArch64 ABI documents only allow them for ET_EXEC (non-PIE) objects. See IHI0056B and
      // IHI0044F.
      DL_ERR("%s COPY relocations are not supported", relocator.si->get_realpath());
      return false;
    case R_GENERIC_TLS_TPREL:
      count_relocation_if<IsGeneral>(kRelocRelative);
      {
        ElfW(Addr) tpoff = 0;
        if (found_in == nullptr) {
          // Unresolved weak relocation. Leave tpoff at 0 to resolve
          // &weak_tls_symbol to __get_tls().
        } else {
          CHECK(found_in->get_tls() != nullptr); // We rejected a missing TLS segment above.
          const TlsModule& mod = get_tls_module(found_in->get_tls()->module_id);
          if (mod.static_offset != SIZE_MAX) {
            tpoff += mod.static_offset - relocator.tls_tp_base;
          } else {
            DL_ERR("TLS symbol \"%s\" in dlopened \"%s\" referenced from \"%s\" using IE access model",
                   sym_name, found_in->get_realpath(), relocator.si->get_realpath());
            return false;
          }
        }
        tpoff += sym_addr + get_addend_rel();
        trace_reloc("RELO TLS_TPREL %16p <- %16p %s",
                    rel_target, reinterpret_cast<void*>(tpoff), sym_name);
        *static_cast<ElfW(Addr)*>(rel_target) = tpoff;
      }
      break;
    case R_GENERIC_TLS_DTPMOD:
      count_relocation_if<IsGeneral>(kRelocRelative);
      {
        size_t module_id = 0;
        if (found_in == nullptr) {
          // Unresolved weak relocation. Evaluate the module ID to 0.
        } else {
          CHECK(found_in->get_tls() != nullptr); // We rejected a missing TLS segment above.
          module_id = found_in->get_tls()->module_id;
        }
        trace_reloc("RELO TLS_DTPMOD %16p <- %zu %s",
                    rel_target, module_id, sym_name);
        *static_cast<ElfW(Addr)*>(rel_target) = module_id;
      }
      break;
    case R_GENERIC_TLS_DTPREL:
      count_relocation_if<IsGeneral>(kRelocRelative);
      {
        const ElfW(Addr) result = sym_addr + get_addend_rel() - TLS_DTV_OFFSET;
        trace_reloc("RELO TLS_DTPREL %16p <- %16p %s",
                    rel_target, reinterpret_cast<void*>(result), sym_name);
        *static_cast<ElfW(Addr)*>(rel_target) = result;
      }
      break;

#if defined(__aarch64__)
    // Bionic currently only implements TLSDESC for arm64. This implementation should work with
    // other architectures, as long as the resolver functions are implemented.
    case R_GENERIC_TLSDESC:
      count_relocation_if<IsGeneral>(kRelocRelative);
      {
        ElfW(Addr) addend = reloc.r_addend;
        TlsDescriptor* desc = static_cast<TlsDescriptor*>(rel_target);
        if (found_in == nullptr) {
          // Unresolved weak relocation.
          desc->func = tlsdesc_resolver_unresolved_weak;
          desc->arg = addend;
          trace_reloc("RELO TLSDESC %16p <- unresolved weak, addend 0x%zx %s",
                      rel_target, static_cast<size_t>(addend), sym_name);
        } else {
          CHECK(found_in->get_tls() != nullptr); // We rejected a missing TLS segment above.
          size_t module_id = found_in->get_tls()->module_id;
          const TlsModule& mod = get_tls_module(module_id);
          if (mod.static_offset != SIZE_MAX) {
            desc->func = tlsdesc_resolver_static;
            desc->arg = mod.static_offset - relocator.tls_tp_base + sym_addr + addend;
            trace_reloc("RELO TLSDESC %16p <- static (0x%zx - 0x%zx + 0x%zx + 0x%zx) %s",
                        rel_target, mod.static_offset, relocator.tls_tp_base,
                        static_cast<size_t>(sym_addr), static_cast<size_t>(addend),
                        sym_name);
          } else {
            relocator.tlsdesc_args->push_back({
              .generation = mod.first_generation,
              .index.module_id = module_id,
              .index.offset = sym_addr + addend,
            });
            // Defer the TLSDESC relocation until the address of the TlsDynamicResolverArg object
            // is finalized.
            relocator.deferred_tlsdesc_relocs.push_back({
              desc, relocator.tlsdesc_args->size() - 1
            });
            const TlsDynamicResolverArg& desc_arg = relocator.tlsdesc_args->back();
            trace_reloc("RELO TLSDESC %16p <- dynamic (gen %zu, mod %zu, off %zu) %s",
                        rel_target, desc_arg.generation, desc_arg.index.module_id,
                        desc_arg.index.offset, sym_name);
          }
        }
      }
      break;
#endif  // defined(__aarch64__)

#if defined(__x86_64__)
    case R_X86_64_32:
      count_relocation_if<IsGeneral>(kRelocAbsolute);
      {
        const Elf32_Addr result = sym_addr + reloc.r_addend;
        trace_reloc("RELO R_X86_64_32 %16p <- 0x%08x %s",
                    rel_target, result, sym_name);
        // 重定位操作
        *static_cast<Elf32_Addr*>(rel_target) = result;
      }
      break;
    case R_X86_64_PC32:
      count_relocation_if<IsGeneral>(kRelocRelative);
      {
        const ElfW(Addr) target = sym_addr + reloc.r_addend;
        const ElfW(Addr) base = reinterpret_cast<ElfW(Addr)>(rel_target);
        const Elf32_Addr result = target - base;
        trace_reloc("RELO R_X86_64_PC32 %16p <- 0x%08x (%16p - %16p) %s",
                    rel_target, result, reinterpret_cast<void*>(target),
                    reinterpret_cast<void*>(base), sym_name);
        // 类似R_X86_64_32
        *static_cast<Elf32_Addr*>(rel_target) = result;
      }
      break;
#elif defined(__i386__)
    case R_386_PC32:
      count_relocation_if<IsGeneral>(kRelocRelative);
      {
        const ElfW(Addr) target = sym_addr + get_addend_rel();
        const ElfW(Addr) base = reinterpret_cast<ElfW(Addr)>(rel_target);
        const ElfW(Addr) result = target - base;
        trace_reloc("RELO R_386_PC32 %16p <- 0x%08x (%16p - %16p) %s",
                    rel_target, result, reinterpret_cast<void*>(target),
                    reinterpret_cast<void*>(base), sym_name);
        *static_cast<ElfW(Addr)*>(rel_target) = result;
      }
      break;
#endif
    default:
      DL_ERR("unknown reloc type %d in \"%s\"", r_type, relocator.si->get_realpath());
      return false;
  }
  return true;
}
