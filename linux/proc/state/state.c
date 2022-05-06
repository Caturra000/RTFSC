// 文件：/include/linux/sched.h


// 进程的状态分为运行状态和退出状态，分别用TASK_和EXIT_前缀来标记
// 但是这些字段本身含义也复杂，有些是内部用的，有些是复合状态
// 而且源码给出字段的顺序并不是按照功能/含义区分的，而是按照历史添加顺序直接贴上去的orz
// 我觉得看task_state_array给出的解释会好一点


/*
 * Task state bitmask. NOTE! These bits are also
 * encoded in fs/proc/array.c: get_task_state().
 *
 * We have two separate sets of flags: task->state
 * is about runnability, while task->exit_state are
 * about the task exiting. Confusing, but this way
 * modifying one set can't modify the other one by
 * mistake.
 */

/* Used in tsk->state: */
#define TASK_RUNNING			0x0000
#define TASK_INTERRUPTIBLE		0x0001
#define TASK_UNINTERRUPTIBLE		0x0002
#define __TASK_STOPPED			0x0004
#define __TASK_TRACED			0x0008
/* Used in tsk->exit_state: */
#define EXIT_DEAD			0x0010
#define EXIT_ZOMBIE			0x0020
#define EXIT_TRACE			(EXIT_ZOMBIE | EXIT_DEAD)
/* Used in tsk->state again: */
#define TASK_PARKED			0x0040
#define TASK_DEAD			0x0080
#define TASK_WAKEKILL			0x0100
#define TASK_WAKING			0x0200
#define TASK_NOLOAD			0x0400
#define TASK_NEW			0x0800
#define TASK_STATE_MAX			0x1000

/* Convenience macros for the sake of set_current_state: */
#define TASK_KILLABLE			(TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)
#define TASK_STOPPED			(TASK_WAKEKILL | __TASK_STOPPED)
#define TASK_TRACED			(TASK_WAKEKILL | __TASK_TRACED)

#define TASK_IDLE			(TASK_UNINTERRUPTIBLE | TASK_NOLOAD)

/* Convenience macros for the sake of wake_up(): */
#define TASK_NORMAL			(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)

/* get_task_state(): */
#define TASK_REPORT			(TASK_RUNNING | TASK_INTERRUPTIBLE | \
					 TASK_UNINTERRUPTIBLE | __TASK_STOPPED | \
					 __TASK_TRACED | EXIT_DEAD | EXIT_ZOMBIE | \
					 TASK_PARKED)




// 文件：/fs/proc/array.c
static inline const char *get_task_state(struct task_struct *tsk)
{
	BUILD_BUG_ON(1 + ilog2(TASK_REPORT_MAX) != ARRAY_SIZE(task_state_array));
	return task_state_array[task_state_index(tsk)];
}

static inline unsigned int task_state_index(struct task_struct *tsk)
{
	unsigned int tsk_state = READ_ONCE(tsk->state);
	unsigned int state = (tsk_state | tsk->exit_state) & TASK_REPORT;

	BUILD_BUG_ON_NOT_POWER_OF_2(TASK_REPORT_MAX);

	if (tsk_state == TASK_IDLE)
		state = TASK_REPORT_IDLE;

	// 位运算，find last set bit in word
	// 我觉得位运算说first / last是很模糊的，
	// 因为总有人习惯是从高位到低位看。。
	// 这里说的是最高位的1
	return fls(state);
}

// 文件：/fs/proc/array.c
/*
 * The task state array is a strange "bitmap" of
 * reasons to sleep. Thus "running" is zero, and
 * you can test for combinations of others with
 * simple bit tests.
 */
// 这是用户层面能看到的状态，远比内核定义的少
// debug了几个状态以供参考：
// - TASK_NEW：返回R（R已经相当包容了，新生、就绪、已运行都在这里）
// - TASK_KILLABLE：返回D（所以说不是所有D都能有kill免死金牌）
// - TASK_IDLE：返回D（不是I，I如下面注释所说是超出REPORT范围的取值，但是TASK_IDLE我不知道用在哪里）
static const char * const task_state_array[] = {

	/* states in TASK_REPORT: */
	"R (running)",		/* 0x00 */
	"S (sleeping)",		/* 0x01 */
	"D (disk sleep)",	/* 0x02 */
	"T (stopped)",		/* 0x04 */
	"t (tracing stop)",	/* 0x08 */
	"X (dead)",		/* 0x10 */
	"Z (zombie)",		/* 0x20 */
	"P (parked)",		/* 0x40 */

	/* states beyond TASK_REPORT: */
	"I (idle)",		/* 0x80 */
};

// 顺便贴上魔改过的debug小demo，直接在用户态下编译输出就好了
///////////////////////////////////////////////////////////////
// #include <stdio.h>
//
// // 鉴于看代码太累，直接从源码抠出来调试吧
//
// static const char * const task_state_array[] = {
//
// 	/* states in TASK_REPORT: */
// 	"R (running)",		/* 0x00 */
// 	"S (sleeping)",		/* 0x01 */
// 	"D (disk sleep)",	/* 0x02 */
// 	"T (stopped)",		/* 0x04 */
// 	"t (tracing stop)",	/* 0x08 */
// 	"X (dead)",		/* 0x10 */
// 	"Z (zombie)",		/* 0x20 */
// 	"P (parked)",		/* 0x40 */
//
// 	/* states beyond TASK_REPORT: */
// 	"I (idle)",		/* 0x80 */
// };
//
// #define TASK_RUNNING			0x0000
// #define TASK_INTERRUPTIBLE		0x0001
// #define TASK_UNINTERRUPTIBLE		0x0002
// #define __TASK_STOPPED			0x0004
// #define __TASK_TRACED			0x0008
// /* Used in tsk->exit_state: */
// #define EXIT_DEAD			0x0010
// #define EXIT_ZOMBIE			0x0020
// #define EXIT_TRACE			(EXIT_ZOMBIE | EXIT_DEAD)
// /* Used in tsk->state again: */
// #define TASK_PARKED			0x0040
// #define TASK_DEAD			0x0080
// #define TASK_WAKEKILL			0x0100
// #define TASK_WAKING			0x0200
// #define TASK_NOLOAD			0x0400
// #define TASK_NEW			0x0800
// #define TASK_STATE_MAX			0x1000
//
// /* Convenience macros for the sake of set_current_state: */
// #define TASK_KILLABLE			(TASK_WAKEKILL | TASK_UNINTERRUPTIBLE)
// #define TASK_STOPPED			(TASK_WAKEKILL | __TASK_STOPPED)
// #define TASK_TRACED			(TASK_WAKEKILL | __TASK_TRACED)
//
// #define TASK_IDLE			(TASK_UNINTERRUPTIBLE | TASK_NOLOAD)
//
// /* Convenience macros for the sake of wake_up(): */
// #define TASK_NORMAL			(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)
//
// /* get_task_state(): */
// #define TASK_REPORT			(TASK_RUNNING | TASK_INTERRUPTIBLE | \
// 					 TASK_UNINTERRUPTIBLE | __TASK_STOPPED | \
// 					 __TASK_TRACED | EXIT_DEAD | EXIT_ZOMBIE | \
// 					 TASK_PARKED)
//
//
// #define TASK_REPORT_IDLE	(TASK_REPORT + 1)
//
//
// // 这里只使用x86-64的实现
// // 其它平台需要额外重写
// static __always_inline int fls(int x)
// {
// 	int r;
//
// 	/*
// 	 * AMD64 says BSRL won't clobber the dest reg if x==0; Intel64 says the
// 	 * dest reg is undefined if x==0, but their CPU architect says its
// 	 * value is written to set it to the same as before, except that the
// 	 * top 32 bits will be cleared.
// 	 *
// 	 * We cannot do this on 32 bits because at the very least some
// 	 * 486 CPUs did not behave this way.
// 	 */
// 	asm("bsrl %1,%0"
// 	    : "=r" (r)
// 	    : "rm" (x), "0" (-1));
//
// 	return r + 1;
// }
//
//
// // 这里作出修改，直接读取state
// static inline unsigned int task_state_index(unsigned int state)
// {
// 	state &= TASK_REPORT;
//
// 	if (state == TASK_IDLE)
// 		state = TASK_REPORT_IDLE;
//
// 	return fls(state);
// }
//
// static inline void print(unsigned int state) {
//     printf("%s\n", task_state_array[task_state_index(state)]);
// }
//
// int main() {
//     // R
//     print(TASK_NEW);
//
//     // D
//     print(TASK_IDLE);
//
//     // D
//     print(TASK_KILLABLE);
// }
///////////////////////////////////////////////////////////////
