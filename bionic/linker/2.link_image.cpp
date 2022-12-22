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
  set_image_linked();
  return true;
}

// 文件：bionic/linker/linker_relocate.cpp
bool soinfo::relocate(const SymbolLookupList& lookup_list) {

  VersionTracker version_tracker;

  if (!version_tracker.init(this)) {
    return false;
  }

  Relocator relocator(version_tracker, lookup_list);
  relocator.si = this;
  relocator.si_strtab = strtab_;
  relocator.si_strtab_size = has_min_version(1) ? strtab_size_ : SIZE_MAX;
  relocator.si_symtab = symtab_;
  relocator.tlsdesc_args = &tlsdesc_args_;
  relocator.tls_tp_base = __libc_shared_globals()->static_tls_layout.offset_thread_pointer();

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

  if (relr_ != nullptr) {
    DEBUG("[ relocating %s relr ]", get_realpath());
    if (!relocate_relr()) {
      return false;
    }
  }

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
  if (rel_ != nullptr) {
    DEBUG("[ relocating %s rel ]", get_realpath());
    if (!plain_relocate<RelocMode::Typical>(relocator, rel_, rel_count_)) {
      return false;
    }
  }
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