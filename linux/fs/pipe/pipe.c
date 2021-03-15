// fs/pipe.c


/* No kernel lock held - fine */
static __poll_t
pipe_poll(struct file *filp, poll_table *wait)
{
	__poll_t mask;
	struct pipe_inode_info *pipe = filp->private_data;
	int nrbufs;

	poll_wait(filp, &pipe->wait, wait);

	/* Reading only -- no need for acquiring the semaphore.  */
	nrbufs = pipe->nrbufs;
	mask = 0;
	if (filp->f_mode & FMODE_READ) {
		mask = (nrbufs > 0) ? EPOLLIN | EPOLLRDNORM : 0;
		if (!pipe->writers && filp->f_version != pipe->w_counter)
			mask |= EPOLLHUP;
	}

	if (filp->f_mode & FMODE_WRITE) {
		mask |= (nrbufs < pipe->buffers) ? EPOLLOUT | EPOLLWRNORM : 0;
		/*
		 * Most Unices do not set EPOLLERR for FIFOs but on Linux they
		 * behave exactly like pipes for poll().
		 */
		if (!pipe->readers)
			mask |= EPOLLERR;
	}

	return mask;
}