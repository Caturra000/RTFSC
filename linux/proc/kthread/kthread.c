// 文件：/kernel/kthread.c
static int kthread(void *_create)
{
	/* Copy data: it's on kthread's stack */
	struct kthread_create_info *create = _create;
	int (*threadfn)(void *data) = create->threadfn;
	void *data = create->data;
	struct completion *done;
	struct kthread *self;
	int ret;

	self = kzalloc(sizeof(*self), GFP_KERNEL);
	set_kthread_struct(self);

	/* If user was SIGKILLed, I release the structure. */
	done = xchg(&create->done, NULL);
	if (!done) {
		kfree(create);
		do_exit(-EINTR);
	}

	if (!self) {
		create->result = ERR_PTR(-ENOMEM);
		complete(done);
		do_exit(-ENOMEM);
	}

	self->data = data;
	init_completion(&self->exited);
	init_completion(&self->parked);
	current->vfork_done = &self->exited;

	/* OK, tell user we're spawned, wait for stop or wakeup */
	// kthread在切出前会设为UNINTERRUPTIBLE
	__set_current_state(TASK_UNINTERRUPTIBLE);
	// 当对端通过done得知已经初始化完成后（如wait_for_completion），可以通过create->result得到当前的kthread
	create->result = current;
	complete(done);
	schedule();

	ret = -EINTR;
	if (!test_bit(KTHREAD_SHOULD_STOP, &self->flags)) {
		cgroup_kthread_ready();
		__kthread_parkme(self);
		// 切回来后就真正执行threadfn
		ret = threadfn(data);
	}
	do_exit(ret);
}
