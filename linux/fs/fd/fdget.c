
static inline struct fd fdget_pos(int fd)
{
	return __to_fd(__fdget_pos(fd));
}

unsigned long __fdget_pos(unsigned int fd)
{
	unsigned long v = __fdget(fd);
	struct file *file = (struct file *)(v & ~3);

	if (file && (file->f_mode & FMODE_ATOMIC_POS)) {
		if (file_count(file) > 1) { // #define file_count(x)	atomic_long_read(&(x)->f_count)
			v |= FDPUT_POS_UNLOCK;
			mutex_lock(&file->f_pos_lock);
		}
	}
	return v;
}

unsigned long __fdget(unsigned int fd)
{
	return __fget_light(fd, FMODE_PATH);
}


/*
 * Lightweight file lookup - no refcnt increment if fd table isn't shared.
 *
 * You can use this instead of fget if you satisfy all of the following
 * conditions:
 * 1) You must call fput_light before exiting the syscall and returning control
 *    to userspace (i.e. you cannot remember the returned struct file * after
 *    returning to userspace).
 * 2) You must not call filp_close on the returned struct file * in between
 *    calls to fget_light and fput_light.
 * 3) You must not clone the current task in between the calls to fget_light
 *    and fput_light.
 *
 * The fput_needed flag returned by fget_light should be passed to the
 * corresponding fput_light.
 */
// 看注释应该是一个refcount只有0或者1的file lookup流程
// mask可能是FMODE_PATH，File is opened with O_PATH
static unsigned long __fget_light(unsigned int fd, fmode_t mask)
{
	struct files_struct *files = current->files;
	struct file *file;

	if (atomic_read(&files->count) == 1) {
		file = __fcheck_files(files, fd); // 即使是下面的refcount==0的情况，最终也是调用到__fcheck_files
		if (!file || unlikely(file->f_mode & mask))
			return 0;
		return (unsigned long)file;
	} else {
		file = __fget(fd, mask);
		if (!file)
			return 0;
		// FDPUT_FPUT使得fdput()/fput()调用有效
		// fget函数从文件描述符表中读取文件描述符时，会增加文件描述符的引用计数f_count。相对应的，fput函数会减少f_count
		// ref: https://xz.aliyun.com/t/4494
		return FDPUT_FPUT | (unsigned long)file;
	}
}

/*
 * The caller must ensure that fd table isn't shared or hold rcu or file lock
 */
static inline struct file *__fcheck_files(struct files_struct *files, unsigned int fd)
{
	struct fdtable *fdt = rcu_dereference_raw(files->fdt);

	if (fd < fdt->max_fds) {
		fd = array_index_nospec(fd, fdt->max_fds); // 可以认为是一些边界检查
		return rcu_dereference_raw(fdt->fd[fd]);
	}
	return NULL;
}

static struct file *__fget(unsigned int fd, fmode_t mask)
{
	struct files_struct *files = current->files;
	struct file *file;

	rcu_read_lock();
loop:
	file = fcheck_files(files, fd);
	if (file) {
		/* File object ref couldn't be taken.
		 * dup2() atomicity guarantee is the reason
		 * we loop to catch the new file (or NULL pointer)
		 */
		if (file->f_mode & mask)
			file = NULL;
		else if (!get_file_rcu(file)) // #define get_file_rcu(x) atomic_long_inc_not_zero(&(x)->f_count)
			goto loop;
	}
	rcu_read_unlock();

	return file;
}

static inline struct file *fcheck_files(struct files_struct *files, unsigned int fd)
{
	RCU_LOCKDEP_WARN(!rcu_read_lock_held() &&
			   !lockdep_is_held(&files->file_lock),
			   "suspicious rcu_dereference_check() usage");
	return __fcheck_files(files, fd);
}



static inline struct fd __to_fd(unsigned long v)
{
	return (struct fd){(struct file *)(v & ~3),v & 3};
}