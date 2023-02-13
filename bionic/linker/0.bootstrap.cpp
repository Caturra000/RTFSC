// 文件：bionic/linker/arch/x86_64/begin.S
// Android (Dynamic) Linker在可执行文件载入时根据ELF的INTERP先启动自身
// 经过系统调用后，控制流由kernel传入到`_start`函数
// 在跳转后，由%rsp指向的栈有以下信息（大概）
// argc, argv[0...n), null, envp[0...term), null, auxv[0...term]
ENTRY(_start)
  // Force unwinds to end in this function.
  .cfi_undefined %rip

  mov %rsp, %rdi
  // 跳转到__linker_init，
  // rdi参数既栈顶，因此从传参raw_args能得知argc, argv, envp和auxv
  call __linker_init

  /* linker init returns (%rax) the _entry address in the main image */
  jmp *%rax
END(_start)




// 文件：bionic/linker/linker_main.cpp

/*
 * This is the entry point for the linker, called from begin.S. This
 * method is responsible for fixing the linker's own relocations, and
 * then calling __linker_init_post_relocation().
 *
 * Because this method is called before the linker has fixed it's own
 * relocations, any attempt to reference an extern variable, extern
 * function, or other GOT reference will generate a segfault.
 */
// 链接器本身（也作为一个.so）需要外部链接（extern）的部分在未自举前无法使用
// 所以在源码中多用static来规避问题，并且不引入依赖库
//
// 在实现上，为了使用非静态函数，linker将这部分源码从.o生成.a，
// 然后在调用lld使用-shared生成.so时，使用-Bstatic选项进行静态链接
// 但是非静态的（全局）变量仍然需要GOT，目前无法使用
extern "C" ElfW(Addr) __linker_init(void* raw_args) {
  // Initialize TLS early so system calls and errno work.
  // 比如errno是threadlocal，所以要先处理（一部分）TLS问题
  //
  // 这里`args`用于解析%rdi传递得到的raw_args
  KernelArgumentBlock args(raw_args);
  bionic_tcb temp_tcb __attribute__((uninitialized));
  // 由于memset有ifunc限制暂时无法使用，这里使用一个相对低效的实现
  // TODO ifunc https://jasoncc.github.io/gnu_gcc_glibc/gnu-ifunc.html
  linker_memclr(&temp_tcb, sizeof(temp_tcb));
  __libc_init_main_thread_early(args, &temp_tcb);

  // When the linker is run by itself (rather than as an interpreter for
  // another program), AT_BASE is 0.
  // 关于auxv可以查看man getauxval，通过读取proc文件系统获得
  // 关于AT_BASE：The base address of the program interpreter (usually, the dynamic linker).
  ElfW(Addr) linker_addr = getauxval(AT_BASE);
  if (linker_addr == 0) {
    // The AT_PHDR and AT_PHNUM aux values describe this linker instance, so use
    // the phdr to find the linker's base address.
    ElfW(Addr) load_bias;
    // 通过查找PT_PHDR获取linker_addr
    get_elf_base_from_phdr(
      reinterpret_cast<ElfW(Phdr)*>(getauxval(AT_PHDR)), getauxval(AT_PHNUM),
      &linker_addr, &load_bias);
  }

  // 算出ehdr和phdr
  ElfW(Ehdr)* elf_hdr = reinterpret_cast<ElfW(Ehdr)*>(linker_addr);
  ElfW(Phdr)* phdr = reinterpret_cast<ElfW(Phdr)*>(linker_addr + elf_hdr->e_phoff);

  // string.h functions must not be used prior to calling the linker's ifunc resolvers.
  call_ifunc_resolvers();

  // TODO soinfo会用于dlopen内部实现
  // 目前soinfo没法完成堆管理，先用栈暂存，自举完成后再搬过去
  soinfo tmp_linker_so(nullptr, nullptr, nullptr, 0, 0);

  tmp_linker_so.base = linker_addr;
  tmp_linker_so.size = phdr_table_get_load_size(phdr, elf_hdr->e_phnum);
  tmp_linker_so.load_bias = get_elf_exec_load_bias(elf_hdr);
  tmp_linker_so.dynamic = nullptr;
  tmp_linker_so.phdr = phdr;
  tmp_linker_so.phnum = elf_hdr->e_phnum;
  tmp_linker_so.set_linker_flag();

  // Prelink the linker so we can access linker globals.
  // 目前无法访问外部变量
  // 需要先完成自举过程
  if (!tmp_linker_so.prelink_image()) __linker_cannot_link(args.argv[0]);
  // TODO SymbolLookupList类型
  // 参考：https://cs.android.com/android/platform/superproject/+/android-13.0.0_r18:bionic/linker/linker_soinfo.h;l=97
  if (!tmp_linker_so.link_image(SymbolLookupList(&tmp_linker_so), &tmp_linker_so, nullptr, nullptr)) __linker_cannot_link(args.argv[0]);

  // 在__linker_init_post_relocation前已完成linker自举
  return __linker_init_post_relocation(args, tmp_linker_so);
}


/*
 * This code is called after the linker has linked itself and fixed its own
 * GOT. It is safe to make references to externs and other non-local data at
 * this point. The compiler sometimes moves GOT references earlier in a
 * function, so avoid inlining this function (http://b/80503879).
 */
static ElfW(Addr) __attribute__((noinline))
__linker_init_post_relocation(KernelArgumentBlock& args, soinfo& tmp_linker_so) {
  // Finish initializing the main thread.
  __libc_init_main_thread_late();

  // We didn't protect the linker's RELRO pages in link_image because we
  // couldn't make system calls on x86 at that point, but we can now...
  if (!tmp_linker_so.protect_relro()) __linker_cannot_link(args.argv[0]);

  // And we can set VMA name for the bss section now
  set_bss_vma_name(&tmp_linker_so);

  // Initialize the linker's static libc's globals
  __libc_init_globals();

  // Initialize the linker's own global variables
  tmp_linker_so.call_constructors();

  // Setting the linker soinfo's soname can allocate heap memory, so delay it until here.
  for (const ElfW(Dyn)* d = tmp_linker_so.dynamic; d->d_tag != DT_NULL; ++d) {
    if (d->d_tag == DT_SONAME) {
      tmp_linker_so.set_soname(tmp_linker_so.get_string(d->d_un.d_val));
    }
  }

  // When the linker is run directly rather than acting as PT_INTERP, parse
  // arguments and determine the executable to load. When it's instead acting
  // as PT_INTERP, AT_ENTRY will refer to the loaded executable rather than the
  // linker's _start.
  const char* exe_to_load = nullptr;
  if (getauxval(AT_ENTRY) == reinterpret_cast<uintptr_t>(&_start)) {
    if (args.argc == 3 && !strcmp(args.argv[1], "--list")) {
      // We're being asked to behave like ldd(1).
      g_is_ldd = true;
      exe_to_load = args.argv[2];
    } else if (args.argc <= 1 || !strcmp(args.argv[1], "--help")) {
      async_safe_format_fd(STDOUT_FILENO,
         "Usage: %s [--list] PROGRAM [ARGS-FOR-PROGRAM...]\n"
         "       %s [--list] path.zip!/PROGRAM [ARGS-FOR-PROGRAM...]\n"
         "\n"
         "A helper program for linking dynamic executables. Typically, the kernel loads\n"
         "this program because it's the PT_INTERP of a dynamic executable.\n"
         "\n"
         "This program can also be run directly to load and run a dynamic executable. The\n"
         "executable can be inside a zip file if it's stored uncompressed and at a\n"
         "page-aligned offset.\n"
         "\n"
         "The --list option gives behavior equivalent to ldd(1) on other systems.\n",
         args.argv[0], args.argv[0]);
      _exit(EXIT_SUCCESS);
    } else {
      exe_to_load = args.argv[1];
      __libc_shared_globals()->initial_linker_arg_count = 1;
    }
  }

  // store argc/argv/envp to use them for calling constructors
  g_argc = args.argc - __libc_shared_globals()->initial_linker_arg_count;
  g_argv = args.argv + __libc_shared_globals()->initial_linker_arg_count;
  g_envp = args.envp;
  __libc_shared_globals()->init_progname = g_argv[0];

  // Initialize static variables. Note that in order to
  // get correct libdl_info we need to call constructors
  // before get_libdl_info().
  sonext = solist = solinker = get_libdl_info(tmp_linker_so);
  g_default_namespace.add_soinfo(solinker);

  ElfW(Addr) start_address = linker_main(args, exe_to_load);

  if (g_is_ldd) _exit(EXIT_SUCCESS);

  INFO("[ Jumping to _start (%p)... ]", reinterpret_cast<void*>(start_address));

  // Return the address that the calling assembly stub should jump to.
  return start_address;
}
