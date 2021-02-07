// 文件分布：/include/linux/wait.h

struct __wait_queue {
	unsigned int		flags;
	void			*private;
	wait_queue_func_t	func;
	struct list_head	task_list;
};

typedef struct __wait_queue wait_queue_t;


#define __WAITQUEUE_INITIALIZER(name, tsk) {				\
	.private	= tsk,						\
	.func		= default_wake_function,			\
	.task_list	= { NULL, NULL } }

#define DECLARE_WAITQUEUE(name, tsk)					\
	wait_queue_t name = __WAITQUEUE_INITIALIZER(name, tsk)
