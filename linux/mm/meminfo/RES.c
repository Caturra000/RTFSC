// 番外篇：这个不算meminfo，而是top command line中的RES
// 看着挺简单的就直接放到这里了

// TL;DR 总之top中的RES是从/proc/pid/statm中直接获取的，就不必翻top源码了

// 现在从kernel中看下真正的RES怎么统计

// 文件：/fs/proc/array.c
int proc_pid_statm(struct seq_file *m, struct pid_namespace *ns,
			struct pid *pid, struct task_struct *task)
{
	unsigned long size = 0, resident = 0, shared = 0, text = 0, data = 0;
	struct mm_struct *mm = get_task_mm(task);

	if (mm) {
		size = task_statm(mm, &shared, &text, &data, &resident);
		mmput(mm);
	}
	/*
	 * For quick read, open code by putting numbers directly
	 * expected format is
	 * seq_printf(m, "%lu %lu %lu %lu 0 %lu 0\n",
	 *               size, resident, shared, text, data);
	 */
	seq_put_decimal_ull(m, "", size);
	seq_put_decimal_ull(m, " ", resident);
	seq_put_decimal_ull(m, " ", shared);
	seq_put_decimal_ull(m, " ", text);
	seq_put_decimal_ull(m, " ", 0);
	seq_put_decimal_ull(m, " ", data);
	seq_put_decimal_ull(m, " ", 0);
	seq_putc(m, '\n');

	return 0;
}


// 文件：/fs/proc/task_mmu.c
unsigned long task_statm(struct mm_struct *mm,
			 unsigned long *shared, unsigned long *text,
			 unsigned long *data, unsigned long *resident)
{
	*shared = get_mm_counter(mm, MM_FILEPAGES) +
			get_mm_counter(mm, MM_SHMEMPAGES);
	*text = (PAGE_ALIGN(mm->end_code) - (mm->start_code & PAGE_MASK))
								>> PAGE_SHIFT;
	*data = mm->data_vm + mm->stack_vm;
	*resident = *shared + get_mm_counter(mm, MM_ANONPAGES);
	return mm->total_vm;
}


// 文件：/include/linux/mm_types_task.h
enum {
	MM_FILEPAGES,	/* Resident file mapping pages */
	MM_ANONPAGES,	/* Resident anonymous pages */
	MM_SWAPENTS,	/* Anonymous swap entries */
	MM_SHMEMPAGES,	/* Resident shared memory pages */
	NR_MM_COUNTERS
};


// 可以看出 RES = FILEPAGES + SHMEMPAGES + ANONPAGES
