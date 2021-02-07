// 文件分布：/include/linux/wait.h

struct __wait_queue_head {
	spinlock_t		lock;
	struct list_head	task_list;
};

typedef struct __wait_queue_head wait_queue_head_t;

#define __WAIT_QUEUE_HEAD_INITIALIZER(name) {				\
	.lock		= __SPIN_LOCK_UNLOCKED(name.lock),		\
	.task_list	= { &(name).task_list, &(name).task_list } }

#define DECLARE_WAIT_QUEUE_HEAD(name) \
	wait_queue_head_t name = __WAIT_QUEUE_HEAD_INITIALIZER(name)