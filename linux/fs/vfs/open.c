// 文件：/fs/open.c
// 参考：https://www.kernel.org/doc/html/latest/filesystems/path-lookup.html
// NOTE: 这一部分相当不好读（主要是路径解析细节太多，以至于连主干在哪都难分清楚），
//       建议站在巨人的肩膀上，搭配ULK等书提高方向感，需要了解细节则翻阅对应代码的git commit message

// test cases:
// 1. /
// 2. .
// 3. /abc/def/ghi
// 4. abc/def/ghi
// 5. ../abc/./def
// test case 3 / 4 / 5还区分是否create

SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, umode_t, mode)
{
	if (force_o_largefile()) // BITS_PER_LONG != 32
		flags |= O_LARGEFILE; 
		// O_LARGEFILE: Allow files whose sizes cannot be represented 
		// in an off_t (but can be represented in an off64_t) to be opened.

	return do_sys_open(AT_FDCWD, filename, flags, mode);
}

// 1. build_open_flags 求出open_flags
// 2. getname 从用户空间复制文件路径到内核空间
// 3. get_unused_fd_flags 分配fd
// 4. do_filp_open 主流程
// 5. fsnotify_open 事件通知
// 6. fd_install 更新current的fd[fd]
long do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode) // dfd为AT_FDCWD
{
	// 处理流程的状态机变量
	struct open_flags op;
	// 根据文件打开标志和文件访问标志求出open_flag
	int fd = build_open_flags(flags, mode, &op); /// helper function #1
	struct filename *tmp;

	if (fd)
		return fd;

	// 从用户空间复制文件路径到内核空间
	tmp = getname(filename);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);
	// 分配fd
	fd = get_unused_fd_flags(flags);
	if (fd >= 0) {
		// 核心流程
		struct file *f = do_filp_open(dfd, tmp, &op);
		if (IS_ERR(f)) {
			put_unused_fd(fd);
			fd = PTR_ERR(f);
		} else {
			// TODO notify相关
			fsnotify_open(f);
			fd_install(fd, f);
		}
	}
	putname(tmp);
	return fd;
}

struct file *do_filp_open(int dfd, struct filename *pathname,
		const struct open_flags *op)
{
	struct nameidata nd;
	// 关于lookup flag见解释 ##flag #0
	// 可能有LOOKUP_FOLLOW
	int flags = op->lookup_flags;
	struct file *filp;
	// 初始化nd
	// TODO 有些成员的用途不太明确
	set_nameidata(&nd, dfd, pathname); /// helper function #4
	// LOOKUP_RCU：启用rcu-walk
	filp = path_openat(&nd, op, flags | LOOKUP_RCU);
	if (unlikely(filp == ERR_PTR(-ECHILD)))
		filp = path_openat(&nd, op, flags);
	if (unlikely(filp == ERR_PTR(-ESTALE)))
		filp = path_openat(&nd, op, flags | LOOKUP_REVAL);
	restore_nameidata();
	return filp;
}

// 1. get_empty_filp 获取file指针
// 2. path_init 设置查找的起点，区分/和进程当前目录
// 3. link_path_walk 解析流程，找到父目录和路径名的最后一个分量
// 4. do_last 决定打开还是创建，将调用到vfs_open()
static struct file *path_openat(struct nameidata *nd,
			const struct open_flags *op, unsigned flags)
{
	const char *s;
	struct file *file;
	int opened = 0;
	int error;
	// 函数注释：Find an unused file structure and return a pointer to it.
	file = get_empty_filp();
	if (IS_ERR(file))
		return file;

	file->f_flags = op->open_flag;

	// 既然是unlikely就不关注了
	if (unlikely(file->f_flags & __O_TMPFILE)) {
		error = do_tmpfile(nd, flags, op, file, &opened);
		goto out2;
	}

	if (unlikely(file->f_flags & O_PATH)) {
		error = do_o_path(nd, flags, file);
		if (!error)
			opened |= FILE_OPENED;
		goto out2;
	}

	// 设置查找的起点，nd->path可能为/也可能为进程当前目录
	s = path_init(nd, flags); /// helper function #3
	if (IS_ERR(s)) {
		put_filp(file);
		return ERR_CAST(s);
	}
	// TODO 路径遍历逻辑
	// link_path_walk的核心就是从字符串到dentry的转换（如nd->path.dentry）
	// 虽然过程非常令人头大就是了
	// CASE 1: link_path_walk基本不会处理任何事情就返回0
	while (!(error = link_path_walk(s, nd)) &&
		// do_last处理vfs_open相关
		(error = do_last(nd, file, op, &opened)) > 0) {
		nd->flags &= ~(LOOKUP_OPEN|LOOKUP_CREATE|LOOKUP_EXCL);
		s = trailing_symlink(nd);
		if (IS_ERR(s)) {
			error = PTR_ERR(s);
			break;
		}
	}
	terminate_walk(nd);
out2:
	if (!(opened & FILE_OPENED)) {
		BUG_ON(!error);
		put_filp(file);
	}
	if (unlikely(error)) {
		if (error == -EOPENSTALE) {
			if (flags & LOOKUP_RCU)
				error = -ECHILD;
			else
				error = -ESTALE;
		}
		file = ERR_PTR(error);
	}
	return file;
}


/*
 * Name resolution.
 * This is the basic name resolution function, turning a pathname into
 * the final dentry. We expect 'base' to be positive and a directory.
 *
 * Returns 0 and nd will have valid dentry and mnt on success.
 * Returns error and drops reference to input namei data on failure.
 */
static int link_path_walk(const char *name, struct nameidata *nd)
{
	int err;

	// 多个'/'视为1个
	while (*name=='/')
		name++;
	// CASE 1: 将结束于此
	if (!*name)
		return 0;

	/* At this point we know we have a real path component. */
	for(;;) {
		u64 hash_len;
		int type;
		// 判断进程是否有inode访问权限
		err = may_lookup(nd);
		if (err)
			return err;
		// 路径哈希
		// TODO 这里的hash是怎么实现的
		hash_len = hash_name(nd->path.dentry, name);

		type = LAST_NORM;
		// 虽然没懂hash_name的算法（写的啥玩意啊），但是hashlen_len的注释里阐述了一种很神奇的hash用法
		// 整个hash值是u64的，高32位存储被哈希的字符串的长度
		// 因此，hashlen_len(x)实现就是 x >> 32，直接反推得到长度
		if (name[0] == '.') switch (hashlen_len(hash_len)) {
			case 2:
				if (name[1] == '.') {
					type = LAST_DOTDOT; // ..
					nd->flags |= LOOKUP_JUMPED;
				}
				break;
			case 1:
				type = LAST_DOT; // .
		}
		// 既不是.也不是..
		if (likely(type == LAST_NORM)) {
			struct dentry *parent = nd->path.dentry;
			nd->flags &= ~LOOKUP_JUMPED;
			if (unlikely(parent->d_flags & DCACHE_OP_HASH)) {
				struct qstr this = { { .hash_len = hash_len }, .name = name };
				err = parent->d_op->d_hash(parent, &this);
				if (err < 0)
					return err;
				hash_len = this.hash_len;
				name = this.name;
			}
		}

		nd->last.hash_len = hash_len;
		nd->last.name = name;
		nd->last_type = type;

		name += hashlen_len(hash_len);
		// 可能是\0或者是/
		// 如果是\0
		if (!*name)
			goto OK;
		/*
		 * If it wasn't NUL, we know it was '/'. Skip that
		 * slash, and continue until no more slashes.
		 */
		do {
			name++;
		} while (unlikely(*name == '/'));
		// 跳过若干///后还是\0
		if (unlikely(!*name)) {
OK:
			/* pathname body, done */
			// 一般应该是这里？
			//（如果已经没有别的话，但如果是/blabla/你在这里/blabla？）
			// 忘了进入的条件，上述的说法的话是不符合这个if判断的
			if (!nd->depth)
				return 0;
			// 目测是与symlink相关
			name = nd->stack[nd->depth - 1].name;
			/* trailing symlink, done */
			if (!name)
				return 0;
			/* last component of nested symlink */
			err = walk_component(nd, WALK_FOLLOW);
		} else {
			/* not the last component */
			// 常规的中间状态，如/blabla/你在这里/blabla
			err = walk_component(nd, WALK_FOLLOW | WALK_MORE);
		}
		if (err < 0)
			return err;

		if (err) {
			const char *s = get_link(nd);

			if (IS_ERR(s))
				return PTR_ERR(s);
			err = 0;
			if (unlikely(!s)) {
				/* jumped */
				put_link(nd);
			} else {
				nd->stack[nd->depth - 1].name = name;
				name = s;
				continue;
			}
		}
		if (unlikely(!d_can_lookup(nd->path.dentry))) {
			if (nd->flags & LOOKUP_RCU) {
				// TODO unlazy walk的具体含义？是取消RCU walk吗
				if (unlazy_walk(nd))
					return -ECHILD;
			}
			return -ENOTDIR;
		}
	}
}

// 进入这个函数至少有三种可能
// 1. 是.或者..
// 2. 是符号链接
// 3. 是普通文件，但处于仍在解析的状态（目录）
// 这里优先关注中间状态（就是3），flags会加上WALK_FOLLOW | WALK_MORE
static int walk_component(struct nameidata *nd, int flags)
{
	struct path path;
	struct inode *inode;
	unsigned seq;
	int err;
	/*
	 * "." and ".." are special - ".." especially so because it has
	 * to be able to know about the current root directory and
	 * parent relationships.
	 */
	// 暂不考虑这个流程，麻烦
	if (unlikely(nd->last_type != LAST_NORM)) {
		err = handle_dots(nd, nd->last_type);
		if (!(flags & WALK_MORE) && nd->depth)
			put_link(nd);
		return err;
	}
	// 见 helper function #5
	err = lookup_fast(nd, &path, &inode, &seq);
	// 0也要进入?
	// 从lookup_fast的结构看到，最为likely的情况是return 1
	if (unlikely(err <= 0)) {
		if (err < 0)
			return err;
		// 首先要尝试fast，有问题再slow，可认为slow就是要求具体文件系统自己lookup
		// 会调用到inode->i_op->lookup(inode, dentry, flags)
		path.dentry = lookup_slow(&nd->last, nd->path.dentry,
					  nd->flags);
		if (IS_ERR(path.dentry))
			return PTR_ERR(path.dentry);

		path.mnt = nd->path.mnt;
		err = follow_managed(&path, nd);
		if (unlikely(err < 0))
			return err;

		if (unlikely(d_is_negative(path.dentry))) {
			path_to_nameidata(&path, nd);
			return -ENOENT;
		}

		seq = 0;	/* we are already out of RCU mode */
		inode = d_backing_inode(path.dentry);
	}

	// 这里更多处理symbol link的事情
	// 如果是普通过程，约等于return 0
	// 见 helper function #6
	return step_into(nd, &path, flags, inode, seq);
}



/*
 * Handle the last step of open()
 */
static int do_last(struct nameidata *nd,
		   struct file *file, const struct open_flags *op,
		   int *opened)
{
	// CASE 1: dir就是root
	struct dentry *dir = nd->path.dentry;
	int open_flag = op->open_flag;
	bool will_truncate = (open_flag & O_TRUNC) != 0;
	bool got_write = false;
	int acc_mode = op->acc_mode;
	unsigned seq;
	struct inode *inode;
	struct path path;
	int error;

	// CASE 1: flags = LOOKUP_RCU | LOOKUP_FOLLOW | LOOKUP_JUMPED | LOOKUP_OPEN <| LOOKUP_CREATE>
	nd->flags &= ~LOOKUP_PARENT;
	nd->flags |= op->intent;

	// CASE 1: nd->last_type必然为LAST_ROOT，满足
	if (nd->last_type != LAST_NORM) {
		// CASE 1: 直接返回0
		error = handle_dots(nd, nd->last_type);
		if (unlikely(error))
			return error;
		goto finish_open;
	}

	if (!(open_flag & O_CREAT)) {
		if (nd->last.name[nd->last.len])
			nd->flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
		/* we _can_ be in RCU mode here */
		error = lookup_fast(nd, &path, &inode, &seq);
		if (likely(error > 0))
			goto finish_lookup;

		if (error < 0)
			return error;

		BUG_ON(nd->inode != dir->d_inode);
		BUG_ON(nd->flags & LOOKUP_RCU);
	} else {
		/* create side of things */
		/*
		 * This will *only* deal with leaving RCU mode - LOOKUP_JUMPED
		 * has been cleared when we got to the last component we are
		 * about to look up
		 */
		error = complete_walk(nd);
		if (error)
			return error;

		audit_inode(nd->name, dir, LOOKUP_PARENT);
		/* trailing slashes? */
		if (unlikely(nd->last.name[nd->last.len]))
			return -EISDIR;
	}

	if (open_flag & (O_CREAT | O_TRUNC | O_WRONLY | O_RDWR)) {
		// 必须要write权限
		error = mnt_want_write(nd->path.mnt);
		if (!error)
			got_write = true;
		/*
		 * do _not_ fail yet - we might not need that or fail with
		 * a different error; let lookup_open() decide; we'll be
		 * dropping this one anyway.
		 */
	}
	if (open_flag & O_CREAT)
		inode_lock(dir->d_inode);
	else
		inode_lock_shared(dir->d_inode);
	// TODO
	error = lookup_open(nd, &path, file, op, got_write, opened);
	if (open_flag & O_CREAT)
		inode_unlock(dir->d_inode);
	else
		inode_unlock_shared(dir->d_inode);

	if (error <= 0) {
		if (error)
			goto out;

		if ((*opened & FILE_CREATED) ||
		    !S_ISREG(file_inode(file)->i_mode))
			will_truncate = false;
		// 内部调用__audit_inode：store the inode and device from a lookup
		// inode通过backing_inode(file->f_path.dentry)获取
		audit_inode(nd->name, file->f_path.dentry, 0);
		goto opened;
	}

	if (*opened & FILE_CREATED) {
		/* Don't check for write permission, don't truncate */
		open_flag &= ~O_TRUNC;
		will_truncate = false;
		acc_mode = 0;
		path_to_nameidata(&path, nd);
		goto finish_open_created;
	}

	/*
	 * If atomic_open() acquired write access it is dropped now due to
	 * possible mount and symlink following (this might be optimized away if
	 * necessary...)
	 */
	if (got_write) {
		// 放弃写权限
		mnt_drop_write(nd->path.mnt);
		got_write = false;
	}

	error = follow_managed(&path, nd);
	if (unlikely(error < 0))
		return error;

	if (unlikely(d_is_negative(path.dentry))) {
		path_to_nameidata(&path, nd);
		return -ENOENT;
	}

	/*
	 * create/update audit record if it already exists.
	 */
	audit_inode(nd->name, path.dentry, 0);

	if (unlikely((open_flag & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT))) {
		path_to_nameidata(&path, nd);
		return -EEXIST;
	}

	seq = 0;	/* out of RCU mode, so the value doesn't matter */
	// 通过dentry获取inode
	inode = d_backing_inode(path.dentry);
	// 下面的流程能看清主次了
	// lookup -> open -> open_created -> opened
finish_lookup:
	// 不考虑symbol link的话，有一个关键操作是nd->inode = inode，就是inode开始关联到nd了
	error = step_into(nd, &path, 0, inode, seq);
	if (unlikely(error))
		return error;
finish_open:
	/* Why this, you ask?  _Now_ we might have grown LOOKUP_JUMPED... */
	// CASE 1: nd->root.mnt = NULL，返回0
	error = complete_walk(nd);
	if (error)
		return error;

	// CASE 1: 其实下面流程已经没啥事情了，相当于返回0

	audit_inode(nd->name, nd->path.dentry, 0);
	error = -EISDIR;
	if ((open_flag & O_CREAT) && d_is_dir(nd->path.dentry))
		goto out;
	error = -ENOTDIR;
	if ((nd->flags & LOOKUP_DIRECTORY) && !d_can_lookup(nd->path.dentry))
		goto out;
	if (!d_is_reg(nd->path.dentry))
		will_truncate = false;

	if (will_truncate) {
		error = mnt_want_write(nd->path.mnt);
		if (error)
			goto out;
		got_write = true;
	}
finish_open_created:
	error = may_open(&nd->path, acc_mode, open_flag);
	if (error)
		goto out;
	BUG_ON(*opened & FILE_OPENED); /* once it's opened, it's opened */
	// 这里file会关联到inode，通过nd->path到d_backing_inode(dentry)和file->f_inode关联
	error = vfs_open(&nd->path, file, current_cred());
	if (error)
		goto out;
	*opened |= FILE_OPENED;
opened:
	error = open_check_o_direct(file);
	if (!error)
		error = ima_file_check(file, op->acc_mode, *opened);
	if (!error && will_truncate)
		error = handle_truncate(file);
out:
	if (unlikely(error) && (*opened & FILE_OPENED))
		fput(file);
	if (unlikely(error > 0)) {
		WARN_ON(1);
		error = -EINVAL;
	}
	if (got_write)
		mnt_drop_write(nd->path.mnt);
	return error;
}

/**
 * vfs_open - open the file at the given path
 * @path: path to open
 * @file: newly allocated file with f_flag initialized
 * @cred: credentials to use
 */
int vfs_open(const struct path *path, struct file *file,
	     const struct cred *cred)
{
	struct dentry *dentry = d_real(path->dentry, NULL, file->f_flags, 0);

	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	file->f_path = *path;
	return do_dentry_open(file, d_backing_inode(dentry), NULL, cred);
}

// 1. 调用f_op->open或者指定的open参数
// 2. 预读初始化file_ra_state_init
static int do_dentry_open(struct file *f,
			  struct inode *inode,
			  int (*open)(struct inode *, struct file *),
			  const struct cred *cred)
{
	static const struct file_operations empty_fops = {};
	int error;

	f->f_mode = OPEN_FMODE(f->f_flags) | FMODE_LSEEK |
				FMODE_PREAD | FMODE_PWRITE;

	path_get(&f->f_path);
	f->f_inode = inode;
	f->f_mapping = inode->i_mapping;

	/* Ensure that we skip any errors that predate opening of the file */
	f->f_wb_err = filemap_sample_wb_err(f->f_mapping);

	if (unlikely(f->f_flags & O_PATH)) {
		f->f_mode = FMODE_PATH;
		f->f_op = &empty_fops;
		return 0;
	}

	if (f->f_mode & FMODE_WRITE && !special_file(inode->i_mode)) {
		error = get_write_access(inode);
		if (unlikely(error))
			goto cleanup_file;
		error = __mnt_want_write(f->f_path.mnt);
		if (unlikely(error)) {
			put_write_access(inode);
			goto cleanup_file;
		}
		f->f_mode |= FMODE_WRITER;
	}

	/* POSIX.1-2008/SUSv4 Section XSI 2.9.7 */
	if (S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode))
		f->f_mode |= FMODE_ATOMIC_POS;

	// f_op的初始化
	f->f_op = fops_get(inode->i_fop);
	if (unlikely(WARN_ON(!f->f_op))) {
		error = -ENODEV;
		goto cleanup_all;
	}

	error = security_file_open(f, cred);
	if (error)
		goto cleanup_all;

	error = break_lease(locks_inode(f), f->f_flags);
	if (error)
		goto cleanup_all;

	// 调用不同fs的open接口
	// 当open为NULL时会调用f_op->open
	// 一个通用的例程为generic_file_open和一些额外工作
	if (!open)
		open = f->f_op->open;
	if (open) {
		error = open(inode, f);
		if (error)
			goto cleanup_all;
	}
	if ((f->f_mode & (FMODE_READ | FMODE_WRITE)) == FMODE_READ)
		i_readcount_inc(inode);
	if ((f->f_mode & FMODE_READ) &&
	     likely(f->f_op->read || f->f_op->read_iter))
		f->f_mode |= FMODE_CAN_READ;
	if ((f->f_mode & FMODE_WRITE) &&
	     likely(f->f_op->write || f->f_op->write_iter))
		f->f_mode |= FMODE_CAN_WRITE;

	f->f_write_hint = WRITE_LIFE_NOT_SET;
	f->f_flags &= ~(O_CREAT | O_EXCL | O_NOCTTY | O_TRUNC);

	// 所以f->f_mapping和f->f_mapping->host->i_mapping有啥区别吗
	// 难道file对应的address space和inode对应的address space不一样？
	file_ra_state_init(&f->f_ra, f->f_mapping->host->i_mapping);

	return 0;

cleanup_all:
	fops_put(f->f_op);
	if (f->f_mode & FMODE_WRITER) {
		put_write_access(inode);
		__mnt_drop_write(f->f_path.mnt);
	}
cleanup_file:
	path_put(&f->f_path);
	f->f_path.mnt = NULL;
	f->f_path.dentry = NULL;
	f->f_inode = NULL;
	return error;
}

/*
 * Called when an inode is about to be open.
 * We use this to disallow opening large files on 32bit systems if
 * the caller didn't specify O_LARGEFILE.  On 64bit systems we force
 * on this flag in sys_open.
 */
int generic_file_open(struct inode * inode, struct file * filp)
{
	// 只是打开前的一些简单的检查
	if (!(filp->f_flags & O_LARGEFILE) && i_size_read(inode) > MAX_NON_LFS)
		return -EOVERFLOW;
	return 0;
}



/// helper function #1
static inline int build_open_flags(int flags, umode_t mode, struct open_flags *op)
{
	int lookup_flags = 0;
        // #define ACC_MODE(x) ("\004\002\006\006"[(x)&O_ACCMODE])
        // #define O_ACCMODE	00000003
        // 非常wtf的写法
        // flag: O_RDONLY == 0, O_WRONLY == 1, O_RDWR = 2
	int acc_mode = ACC_MODE(flags);

	/*
	 * Clear out all open flags we don't know about so that we don't report
	 * them in fcntl(F_GETFD) or similar interfaces.
	 */
	flags &= VALID_OPEN_FLAGS;

	if (flags & (O_CREAT | __O_TMPFILE))
		op->mode = (mode & S_IALLUGO) | S_IFREG;
	else // if neither O_CREAT nor O_TMPFILE is specified, then mode is ignored.
		op->mode = 0;

	// 不想看了
	/* Must never be set by userspace */
	flags &= ~FMODE_NONOTIFY & ~O_CLOEXEC;

	/*
	 * O_SYNC is implemented as __O_SYNC|O_DSYNC.  As many places only
	 * check for O_DSYNC if the need any syncing at all we enforce it's
	 * always set instead of having to deal with possibly weird behaviour
	 * for malicious applications setting only __O_SYNC.
	 */
	if (flags & __O_SYNC)
		flags |= O_DSYNC;

	if (flags & __O_TMPFILE) {
		if ((flags & O_TMPFILE_MASK) != O_TMPFILE)
			return -EINVAL;
		if (!(acc_mode & MAY_WRITE))
			return -EINVAL;
	} else if (flags & O_PATH) {
		/*
		 * If we have O_PATH in the open flag. Then we
		 * cannot have anything other than the below set of flags
		 */
		flags &= O_DIRECTORY | O_NOFOLLOW | O_PATH;
		acc_mode = 0;
	}

	op->open_flag = flags;

	/* O_TRUNC implies we need access checks for write permissions */
	if (flags & O_TRUNC)
		acc_mode |= MAY_WRITE;

	/* Allow the LSM permission hook to distinguish append
	   access from general write access. */
	if (flags & O_APPEND)
		acc_mode |= MAY_APPEND;

	op->acc_mode = acc_mode;

	// 关注的LOOKUP1
	op->intent = flags & O_PATH ? 0 : LOOKUP_OPEN;

	if (flags & O_CREAT) {
		// 关注的LOOKUP2
		op->intent |= LOOKUP_CREATE;
		if (flags & O_EXCL)
			op->intent |= LOOKUP_EXCL;
	}

	if (flags & O_DIRECTORY)
		lookup_flags |= LOOKUP_DIRECTORY;
	if (!(flags & O_NOFOLLOW))
		// 关注的LOOKUP3
		lookup_flags |= LOOKUP_FOLLOW;
	op->lookup_flags = lookup_flags;
	// 一般情况的话
	// intent应该有LOOKUP_OPEN，有可能搭上LOOKUP_CREATE
	// lookup_flags有LOOKUP_FOLLOW
	// open_flag基本跟用户传进来的差不多，略有改动
	return 0;
}

/// helper function #2

// 看注释就行了
/* Find an unused file structure and return a pointer to it.
 * Returns an error pointer if some error happend e.g. we over file
 * structures limit, run out of memory or operation is not permitted.
 *
 * Be very careful using this.  You are responsible for
 * getting write access to any mount that you might assign
 * to this filp, if it is opened for write.  If this is not
 * done, you will imbalance int the mount's writer count
 * and a warning at __fput() time.
 */
struct file *get_empty_filp(void)
{
	const struct cred *cred = current_cred();
	static long old_max;
	struct file *f;
	int error;

	/*
	 * Privileged users can go above max_files
	 */
	if (get_nr_files() >= files_stat.max_files && !capable(CAP_SYS_ADMIN)) {
		/*
		 * percpu_counters are inaccurate.  Do an expensive check before
		 * we go and fail.
		 */
		if (percpu_counter_sum_positive(&nr_files) >= files_stat.max_files)
			goto over;
	}

	f = kmem_cache_zalloc(filp_cachep, GFP_KERNEL);
	if (unlikely(!f))
		return ERR_PTR(-ENOMEM);

	percpu_counter_inc(&nr_files);
	f->f_cred = get_cred(cred);
	error = security_file_alloc(f);
	if (unlikely(error)) {
		file_free(f);
		return ERR_PTR(error);
	}

	atomic_long_set(&f->f_count, 1);
	rwlock_init(&f->f_owner.lock);
	spin_lock_init(&f->f_lock);
	mutex_init(&f->f_pos_lock);
	eventpoll_init_file(f);
	/* f->f_version: 0 */
	return f;

over:
	/* Ran out of filps - report that */
	if (get_nr_files() > old_max) {
		pr_info("VFS: file-max limit %lu reached\n", get_max_files());
		old_max = get_nr_files();
	}
	return ERR_PTR(-ENFILE);
}



/// helper function #3

// 如果name以'/'为首，表示绝对路径，nd->path设为当前进程的根目录
// 如果dfd为AT_FDCWD，且名字为相对路径，nd->path设为当前进程的工作目录
// 其他情况，get_fs_pwd
// 当前进程的根目录路径和当前目录路径都在fs_struct中
// 简略地说是根据字符串s和flags来初始化nd的last_type / flags / depth / path / inode等参数
// flags：目前认为有 LOOKUP_RCU | LOOKUP_FOLLOW
static const char *path_init(struct nameidata *nd, unsigned flags)
{
	const char *s = nd->name->name;

	// 什么情况下会没有路径名？
	if (!*s)
		flags &= ~LOOKUP_RCU;

	// 如果成功查找，类型是LAST_NORM（LAST_NORM说明最后一个分量是普通文件名）
	// 如果停留在上一个.，则是LAST_DOT
	// CASE 1:
	// CASE 3:
	// 直到该流程结束只能是LAST_ROOT
	nd->last_type = LAST_ROOT; /* if there are only slashes... */
	// 此处的flags为open_flags->lookup_flags，基本上来自于：用户open时传入的flags和build_open_flags
	// nd->flags认为是 LOOKUP_RCU | LOOKUP_FOLLOW | LOOKUP_JUMPED | LOOKUP_PARENT
	nd->flags = flags | LOOKUP_JUMPED | LOOKUP_PARENT;
	nd->depth = 0;
	// 在/fs/namei.c#filename_lookup中可能会flags |= LOOKUP_ROOT
	// TODO 看下commit message，这个是干什么的（基于根目录的搜索？）
	// 就open流程而言，似乎并没有设置，现在先忽略
	if (flags & LOOKUP_ROOT) {
		struct dentry *root = nd->root.dentry;
		struct inode *inode = root->d_inode;
		if (*s && unlikely(!d_can_lookup(root)))
			return ERR_PTR(-ENOTDIR);
		nd->path = nd->root;
		nd->inode = inode;
		if (flags & LOOKUP_RCU) {
			rcu_read_lock();
			nd->seq = __read_seqcount_begin(&nd->path.dentry->d_seq);
			nd->root_seq = nd->seq;
			nd->m_seq = read_seqbegin(&mount_lock);
		} else {
			path_get(&nd->path);
		}
		return s;
	}

	nd->root.mnt = NULL;
	nd->path.mnt = NULL;
	nd->path.dentry = NULL;

	nd->m_seq = read_seqbegin(&mount_lock);
	// 路径以'/'开头
	// CASE 1:
	// CASE 3:
	if (*s == '/') {
		if (flags & LOOKUP_RCU)
			rcu_read_lock();
		// 设置nd->root为current->fs->root
		set_root(nd);
		// 填充nd->path和nd->inode，因为刚开始来，所以就是刚才set_root()带来的current->fs->root相关的信息
		// 并且打上LOOKUP_JUMPED标记（默认本来就有）
		// '/'开头的流程就到这里了
		if (likely(!nd_jump_root(nd)))
			return s;
		nd->root.mnt = NULL;
		rcu_read_unlock();
		return ERR_PTR(-ECHILD);
	// 一般来说open时dfd是AT_FDCWD（见do_sys_open参数）
	// CASE 2:
	// CASE 4:
	// CASE 5:
	} else if (nd->dfd == AT_FDCWD) {
		// path_openat传入flags时默认设置LOOKUP_RCU
		// 根据current->fs->pwd来初始化nd->path和nd->inode
		if (flags & LOOKUP_RCU) {
			struct fs_struct *fs = current->fs;
			unsigned seq;

			rcu_read_lock();

			do {
				seq = read_seqcount_begin(&fs->seq);
				nd->path = fs->pwd;
				nd->inode = nd->path.dentry->d_inode;
				nd->seq = __read_seqcount_begin(&nd->path.dentry->d_seq);
			} while (read_seqcount_retry(&fs->seq, seq));
		// 不关注这里
		} else {
			get_fs_pwd(current->fs, &nd->path);
			nd->inode = nd->path.dentry->d_inode;
		}
		return s;
	// 如果是openat的话一般不为AT_FDCWD
	// 我们关注open流程就不处理这个了
	} else {
		/* Caller must check execute permissions on the starting path component */
		struct fd f = fdget_raw(nd->dfd);
		struct dentry *dentry;

		if (!f.file)
			return ERR_PTR(-EBADF);

		dentry = f.file->f_path.dentry;

		if (*s) {
			if (!d_can_lookup(dentry)) {
				fdput(f);
				return ERR_PTR(-ENOTDIR);
			}
		}

		nd->path = f.file->f_path;
		if (flags & LOOKUP_RCU) {
			rcu_read_lock();
			nd->inode = nd->path.dentry->d_inode;
			nd->seq = read_seqcount_begin(&nd->path.dentry->d_seq);
		} else {
			path_get(&nd->path);
			nd->inode = nd->path.dentry->d_inode;
		}
		fdput(f);
		return s;
	}
}

/// helper function #4

// TODO
// 填充形参p和更新current->nameidata
static void set_nameidata(struct nameidata *p, int dfd, struct filename *name)
{
	struct nameidata *old = current->nameidata;
	p->stack = p->internal;
	p->dfd = dfd;
	p->name = name;
	p->total_link_count = old ? old->total_link_count : 0;
	p->saved = old;
	current->nameidata = p;
}

/// helper function #5
static int lookup_fast(struct nameidata *nd,
		       struct path *path, struct inode **inode,
		       unsigned *seqp)
{
	struct vfsmount *mnt = nd->path.mnt;
	struct dentry *dentry, *parent = nd->path.dentry;
	int status = 1;
	int err;

	/*
	 * Rename seqlock is not required here because in the off chance
	 * of a false negative due to a concurrent rename, the caller is
	 * going to fall back to non-racy lookup.
	 */
	if (nd->flags & LOOKUP_RCU) {
		unsigned seq;
		bool negative;
		// 可认为是通过找hash得到dentry
		dentry = __d_lookup_rcu(parent, &nd->last, &seq);
		if (unlikely(!dentry)) {
			// 失败的话unlazy walk
			if (unlazy_walk(nd))
				return -ECHILD;
			return 0;
		}

		/*
		 * This sequence count validates that the inode matches
		 * the dentry name information from lookup.
		 */
		// 即使找到dentry了，也会用一系列read_seqcount_retry检查
		*inode = d_backing_inode(dentry);
		negative = d_is_negative(dentry);
		if (unlikely(read_seqcount_retry(&dentry->d_seq, seq)))
			return -ECHILD;

		/*
		 * This sequence count validates that the parent had no
		 * changes while we did the lookup of the dentry above.
		 *
		 * The memory barrier in read_seqcount_begin of child is
		 *  enough, we can use __read_seqcount_retry here.
		 */
		if (unlikely(__read_seqcount_retry(&parent->d_seq, nd->seq)))
			return -ECHILD;

		*seqp = seq;
		status = d_revalidate(dentry, nd->flags);
		if (likely(status > 0)) {
			/*
			 * Note: do negative dentry check after revalidation in
			 * case that drops it.
			 */
			if (unlikely(negative))
				return -ENOENT;
			path->mnt = mnt;
			path->dentry = dentry;
			if (likely(__follow_mount_rcu(nd, path, inode, seqp)))
				// 一般是这里？
				return 1;
		}
		// 简单来说就是rcu-walk不行，try to switch to ref-walk mode
		if (unlazy_child(nd, dentry, seq))
			return -ECHILD;
		if (unlikely(status == -ECHILD))
			/* we'd been told to redo it in non-rcu mode */
			status = d_revalidate(dentry, nd->flags);
	} else {
		dentry = __d_lookup(parent, &nd->last);
		if (unlikely(!dentry))
			return 0;
		status = d_revalidate(dentry, nd->flags);
	}
	if (unlikely(status <= 0)) {
		if (!status)
			d_invalidate(dentry);
		dput(dentry);
		return status;
	}
	if (unlikely(d_is_negative(dentry))) {
		dput(dentry);
		return -ENOENT;
	}

	path->mnt = mnt;
	path->dentry = dentry;
	err = follow_managed(path, nd);
	if (likely(err > 0))
		*inode = d_backing_inode(path->dentry);
	return err;
}

/// helper function #6
/*
 * Do we need to follow links? We _really_ want to be able
 * to do this check without having to look at inode->i_op,
 * so we keep a cache of "no, this doesn't need follow_link"
 * for the common case.
 */
static inline int step_into(struct nameidata *nd, struct path *path,
			    int flags, struct inode *inode, unsigned seq)
{
	if (!(flags & WALK_MORE) && nd->depth)
		put_link(nd);
	if (likely(!d_is_symlink(path->dentry)) ||
	   !(flags & WALK_FOLLOW || nd->flags & LOOKUP_FOLLOW)) {
		/* not a symlink or should not follow */
		path_to_nameidata(path, nd);
		nd->inode = inode;
		nd->seq = seq;
		return 0;
	}
	/* make sure that d_is_symlink above matches inode */
	if (nd->flags & LOOKUP_RCU) {
		if (read_seqcount_retry(&path->dentry->d_seq, seq))
			return -ECHILD;
	}
	return pick_link(nd, path, inode, seq);
}


// ##flag #0
// 关于lookup flag，大体是pathwalk过程的状态机，描述查找的原则
// 如果直接看kernel 4.18里的解释是基本看不懂的，因为改了一些flag但是代码注释又没更新
// ULK里有解释，但是也有一些flag弃用了（路径名查找章节）
// 从github找了新的版本，可能有些flag是5.x才有的，但是起码注释是全的
//
// 其实看这些没啥意思，4.x和5.x改动还是有点大，明白pathwalk是个复杂的过程就好
// 比如LOOKUP_JUMPED，鬼知道要表达什么 https://github.com/torvalds/linux/commit/16c2cd7179881d5dd87779512ca5a0d657c64f62
//
///* pathwalk mode */
// #define LOOKUP_FOLLOW		0x0001	/* follow links at the end */
// #define LOOKUP_DIRECTORY	0x0002	/* require a directory */
// #define LOOKUP_AUTOMOUNT	0x0004  /* force terminal automount */
// #define LOOKUP_EMPTY		0x4000	/* accept empty path [user_... only] */
// #define LOOKUP_DOWN		0x8000	/* follow mounts in the starting point */
// #define LOOKUP_MOUNTPOINT	0x0080	/* follow mounts in the end */

// #define LOOKUP_REVAL		0x0020	/* tell ->d_revalidate() to trust no cache */
// #define LOOKUP_RCU		0x0040	/* RCU pathwalk mode; semi-internal */

// /* These tell filesystem methods that we are dealing with the final component... */
// #define LOOKUP_OPEN		0x0100	/* ... in open */
// #define LOOKUP_CREATE		0x0200	/* ... in object creation */
// #define LOOKUP_EXCL		0x0400	/* ... in exclusive creation */
// #define LOOKUP_RENAME_TARGET	0x0800	/* ... in destination of rename() */

// /* internal use only */
// #define LOOKUP_PARENT		0x0010

// /* Scoping flags for lookup. */
// #define LOOKUP_NO_SYMLINKS	0x010000 /* No symlink crossing. */
// #define LOOKUP_NO_MAGICLINKS	0x020000 /* No nd_jump_link() crossing. */
// #define LOOKUP_NO_XDEV		0x040000 /* No mountpoint crossing. */
// #define LOOKUP_BENEATH		0x080000 /* No escaping from starting point. */
// #define LOOKUP_IN_ROOT		0x100000 /* Treat dirfd as fs root. */
// #define LOOKUP_CACHED		0x200000 /* Only do cached lookup */