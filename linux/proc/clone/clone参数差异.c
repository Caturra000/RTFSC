// 文件分布：/kernel/fork.c



#ifdef __ARCH_WANT_SYS_FORK
SYSCALL_DEFINE0(fork)
{
#ifdef CONFIG_MMU
	return _do_fork(SIGCHLD, 0, 0, NULL, NULL, 0);
#else
	/* can not support in nommu mode */
	return -EINVAL;
#endif
}
#endif







#ifdef __ARCH_WANT_SYS_VFORK
SYSCALL_DEFINE0(vfork)
{
	return _do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, 0,
			0, NULL, NULL, 0);
}
#endif







// pthread库
// 简单的用strace调用看了下pthread_create
// CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID







// kthread
// kthreadd创建时调用pid = kernel_thread(kthread, create, CLONE_FS | CLONE_FILES | SIGCHLD);
// 既CLONE_VM | CLONE_FS | CLONE_FILES | SIGCHLD | CLONE_UNTRACED
pid_t kernel_thread(int (*fn)(void *), void *arg, unsigned long flags)
{
	return _do_fork(flags|CLONE_VM|CLONE_UNTRACED, (unsigned long)fn,
		(unsigned long)arg, NULL, NULL, 0);
}
