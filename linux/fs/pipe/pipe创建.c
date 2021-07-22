// 文件：/fs/pipe.c

SYSCALL_DEFINE1(pipe, int __user *, fildes)
{
	return do_pipe2(fildes, 0);
}

/*
 * sys_pipe() is the normal C calling standard for creating
 * a pipe. It's not the way Unix traditionally does this, though.
 */
static int do_pipe2(int __user *fildes, int flags)
{
	struct file *files[2];
	int fd[2];
	int error;

	error = __do_pipe_flags(fd, files, flags);
	if (!error) {
		if (unlikely(copy_to_user(fildes, fd, sizeof(fd)))) {
			fput(files[0]);
			fput(files[1]);
			put_unused_fd(fd[0]);
			put_unused_fd(fd[1]);
			error = -EFAULT;
		} else {
			fd_install(fd[0], files[0]);
			fd_install(fd[1], files[1]);
		}
	}
	return error;
}

static int __do_pipe_flags(int *fd, struct file **files, int flags)
{
	int error;
	int fdw, fdr;

	if (flags & ~(O_CLOEXEC | O_NONBLOCK | O_DIRECT))
		return -EINVAL;

	error = create_pipe_files(files, flags);
	if (error)
		return error;

	error = get_unused_fd_flags(flags);
	if (error < 0)
		goto err_read_pipe;
	fdr = error;

	error = get_unused_fd_flags(flags);
	if (error < 0)
		goto err_fdr;
	fdw = error;

	audit_fd_pair(fdr, fdw);
	fd[0] = fdr;
	fd[1] = fdw;
	return 0;

 err_fdr:
	put_unused_fd(fdr);
 err_read_pipe:
	fput(files[0]);
	fput(files[1]);
	return error;
}

int create_pipe_files(struct file **res, int flags)
{
	int err;
	struct inode *inode = get_pipe_inode();
	struct file *f;
	struct path path;

	if (!inode)
		return -ENFILE;

	err = -ENOMEM;
	path.dentry = d_alloc_pseudo(pipe_mnt->mnt_sb, &empty_name);
	if (!path.dentry)
		goto err_inode;
	path.mnt = mntget(pipe_mnt);

	d_instantiate(path.dentry, inode);

	f = alloc_file(&path, FMODE_WRITE, &pipefifo_fops);
	if (IS_ERR(f)) {
		err = PTR_ERR(f);
		goto err_dentry;
	}

	f->f_flags = O_WRONLY | (flags & (O_NONBLOCK | O_DIRECT));
	f->private_data = inode->i_pipe;

	res[0] = alloc_file(&path, FMODE_READ, &pipefifo_fops);
	if (IS_ERR(res[0])) {
		err = PTR_ERR(res[0]);
		goto err_file;
	}

	path_get(&path);
	res[0]->private_data = inode->i_pipe;
	res[0]->f_flags = O_RDONLY | (flags & O_NONBLOCK);
	res[1] = f;
	return 0;

err_file:
	put_filp(f);
err_dentry:
	free_pipe_info(inode->i_pipe);
	path_put(&path);
	return err;

err_inode:
	free_pipe_info(inode->i_pipe);
	iput(inode);
	return err;
}

static struct inode * get_pipe_inode(void)
{
	struct inode *inode = new_inode_pseudo(pipe_mnt->mnt_sb);
	struct pipe_inode_info *pipe;

	if (!inode)
		goto fail_inode;

	inode->i_ino = get_next_ino();

	pipe = alloc_pipe_info();
	if (!pipe)
		goto fail_iput;

	inode->i_pipe = pipe;
	pipe->files = 2;
	pipe->readers = pipe->writers = 1;
	inode->i_fop = &pipefifo_fops;

	/*
	 * Mark the inode dirty from the very beginning,
	 * that way it will never be moved to the dirty
	 * list because "mark_inode_dirty()" will think
	 * that it already _is_ on the dirty list.
	 */
	inode->i_state = I_DIRTY;
	inode->i_mode = S_IFIFO | S_IRUSR | S_IWUSR;
	inode->i_uid = current_fsuid();
	inode->i_gid = current_fsgid();
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

	return inode;

fail_iput:
	iput(inode);

fail_inode:
	return NULL;
}


const struct file_operations pipefifo_fops = {
	.open		= fifo_open,
	.llseek		= no_llseek,
	.read_iter	= pipe_read,
	.write_iter	= pipe_write,
	.poll		= pipe_poll,
	.unlocked_ioctl	= pipe_ioctl,
	.release	= pipe_release,
	.fasync		= pipe_fasync,
};