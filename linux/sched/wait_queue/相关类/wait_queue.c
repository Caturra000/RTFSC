// 文件分布：/include/linux/wait.h

struct __wait_queue {
	unsigned int		flags; // WQ_FLAG_EXCLUSIVE  WQ_FLAG_WOKEN
	void			*private; // 指向关联进程的task_struct
	wait_queue_func_t	func;   // wakeup函数   // typedef int (*wait_queue_func_t)(wait_queue_t *wait, unsigned mode, int flags, void *key);
	struct list_head	task_list;
};

typedef struct __wait_queue wait_queue_t;


#define __WAITQUEUE_INITIALIZER(name, tsk) {				\
	.private	= tsk,						\
	.func		= default_wake_function,			\
	.task_list	= { NULL, NULL } }

#define DECLARE_WAITQUEUE(name, tsk)					\
	wait_queue_t name = __WAITQUEUE_INITIALIZER(name, tsk)      // 这里用到了default_wake_function
