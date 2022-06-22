// 文件：/kernel/sched/core.c

/*
 * context_switch - switch to the new MM and the new thread's register state.
 */
// context switch的核心要点已经在注释中说明了
// 就是切换虚拟地址空间、寄存器信息（还有栈）
//
// 在schedule()算法中，挑选出了要切换的prev和next
static __always_inline struct rq *
context_switch(struct rq *rq, struct task_struct *prev,
	       struct task_struct *next, struct rq_flags *rf)
{
	struct mm_struct *mm, *oldmm;

	// 基本上是空实现
	prepare_task_switch(rq, prev, next);

	mm = next->mm;
	oldmm = prev->active_mm;
	/*
	 * For paravirt, this is coupled with an exit in switch_to to
	 * combine the page table reload and the switch backend into
	 * one hypercall.
	 */
	// 半虚拟化相关，略
	arch_start_context_switch(prev);

	/*
	 * If mm is non-NULL, we pass through switch_mm(). If mm is
	 * NULL, we will pass through mmdrop() in finish_task_switch().
	 * Both of these contain the full memory barrier required by
	 * membarrier after storing to rq->curr, before returning to
	 * user-space.
	 */
	// 如果是内核线程的话，mm就是空，需要借active mm
	// 虽然借的是别人的active mm（不能直接借mm，万一对方prev也是内核线程呢？）
	// 但是只用kernel address space而不用user address space，因此随便借
	if (!mm) {
		next->active_mm = oldmm;
		mmgrab(oldmm);
		// 对于内核地址空间，TLB总是能正确翻译虚拟地址VA到PA
		// 但是对于每个mm独立的地址空间那就会造成错误翻译，因此normal process需要switch mm时进一步对TLB做处理
		//
		// lazy TLB mode，与TLB flush有关
		// http://www.wowotech.net/process_management/context-switch-tlb.html
		// 这种内核线程进入lazy TLB mode的意思是：
		// 一个背景：
		// - 当多个CPU各自都在运行同一个mm的task时
		// - 其中一个task修改了对应的地址翻译，那就不只是flush对应CPU的TLB，还需要通过IPI中断方式来通知其它CPU作出修改
		// 另一个背景：
		// - 内核线程借用了其它task的mm，如果没有别的机制，可能被这种无谓的中断莫名其妙地flush掉TLB
		// - 从前面的讨论可以知道，内核线程借用mm时不需要flush
		// 而设置了lazy mode，那就是现在switch不需要额外flush，可以推迟到下一次switch再执行
		enter_lazy_tlb(oldmm, next);
	} else
		// 切换虚拟地址空间，oldmm切为mm
		// 即使切换了mm也不会引起控制流的变化，因为每个进程的kernel映射是一样的
		//
		// Question. 这是不是说明在context switch过程中，内核线程是最为高效的？连基本的mm判断都不需要
		switch_mm_irqs_off(oldmm, mm, next);

	// 更新rq
	if (!prev->mm) {
		prev->active_mm = NULL;
		rq->prev_mm = oldmm;
	}

	rq->clock_update_flags &= ~(RQCF_ACT_SKIP|RQCF_REQ_SKIP);

	// 对rq上锁
	prepare_lock_switch(rq, next, rf);

	/* Here we just switch the register state and the stack. */
	// 注释如上，切换寄存器和栈
	switch_to(prev, next, prev);
	barrier();

	// 对应prepare_task_switch
	return finish_task_switch(prev);
}


// 文件：/arch/x86/mm/tlb.c

// 该函数可能会会执行TLB flush，既物理意义地处理mm切换对于CPU的影响
// 并对CR3 CR4 LDT进行更新
//
// 这个prev参数就没见过哪里有用到
void switch_mm_irqs_off(struct mm_struct *prev, struct mm_struct *next,
			struct task_struct *tsk)
{
	struct mm_struct *real_prev = this_cpu_read(cpu_tlbstate.loaded_mm);
	// asid: address-space identifier
	// 用于TLB中标记虚拟地址空间
	// TODO 有空再看asid怎么维护的吧
	u16 prev_asid = this_cpu_read(cpu_tlbstate.loaded_mm_asid);
	unsigned cpu = smp_processor_id();
	u64 next_tlb_gen;

	/*
	 * NB: The scheduler will call us with prev == next when switching
	 * from lazy TLB mode to normal mode if active_mm isn't changing.
	 * When this happens, we don't assume that CR3 (and hence
	 * cpu_tlbstate.loaded_mm) matches next.
	 *
	 * NB: leave_mm() calls us with prev == NULL and tsk == NULL.
	 */

	/* We don't want flush_tlb_func_* to run concurrently with us. */
	if (IS_ENABLED(CONFIG_PROVE_LOCKING))
		WARN_ON_ONCE(!irqs_disabled());

	/*
	 * Verify that CR3 is what we think it is.  This will catch
	 * hypothetical buggy code that directly switches to swapper_pg_dir
	 * without going through leave_mm() / switch_mm_irqs_off() or that
	 * does something like write_cr3(read_cr3_pa()).
	 *
	 * Only do this check if CONFIG_DEBUG_VM=y because __read_cr3()
	 * isn't free.
	 */
#ifdef CONFIG_DEBUG_VM
	if (WARN_ON_ONCE(__read_cr3() != build_cr3(real_prev->pgd, prev_asid))) {
		/*
		 * If we were to BUG here, we'd be very likely to kill
		 * the system so hard that we don't see the call trace.
		 * Try to recover instead by ignoring the error and doing
		 * a global flush to minimize the chance of corruption.
		 *
		 * (This is far from being a fully correct recovery.
		 *  Architecturally, the CPU could prefetch something
		 *  back into an incorrect ASID slot and leave it there
		 *  to cause trouble down the road.  It's better than
		 *  nothing, though.)
		 */
		__flush_tlb_all();
	}
#endif
	this_cpu_write(cpu_tlbstate.is_lazy, false);

	/*
	 * The membarrier system call requires a full memory barrier and
	 * core serialization before returning to user-space, after
	 * storing to rq->curr. Writing to CR3 provides that full
	 * memory barrier and core serializing instruction.
	 */
	// oldmm和mm相同，基本就return了
	// 对比下面else流程可以了解有无mm switch的性能差距
	if (real_prev == next) {
		VM_WARN_ON(this_cpu_read(cpu_tlbstate.ctxs[prev_asid].ctx_id) !=
			   next->context.ctx_id);

		/*
		 * We don't currently support having a real mm loaded without
		 * our cpu set in mm_cpumask().  We have all the bookkeeping
		 * in place to figure out whether we would need to flush
		 * if our cpu were cleared in mm_cpumask(), but we don't
		 * currently use it.
		 */
		if (WARN_ON_ONCE(real_prev != &init_mm &&
				 !cpumask_test_cpu(cpu, mm_cpumask(next))))
			cpumask_set_cpu(cpu, mm_cpumask(next));

		return;
	} else {
		u16 new_asid;
		bool need_flush;
		u64 last_ctx_id = this_cpu_read(cpu_tlbstate.last_ctx_id);

		/*
		 * Avoid user/user BTB poisoning by flushing the branch
		 * predictor when switching between processes. This stops
		 * one process from doing Spectre-v2 attacks on another.
		 *
		 * As an optimization, flush indirect branches only when
		 * switching into processes that disable dumping. This
		 * protects high value processes like gpg, without having
		 * too high performance overhead. IBPB is *expensive*!
		 *
		 * This will not flush branches when switching into kernel
		 * threads. It will also not flush if we switch to idle
		 * thread and back to the same process. It will flush if we
		 * switch to a different non-dumpable process.
		 */
		if (tsk && tsk->mm &&
		    tsk->mm->context.ctx_id != last_ctx_id &&
		    get_dumpable(tsk->mm) != SUID_DUMP_USER)
			indirect_branch_prediction_barrier();

		if (IS_ENABLED(CONFIG_VMAP_STACK)) {
			/*
			 * If our current stack is in vmalloc space and isn't
			 * mapped in the new pgd, we'll double-fault.  Forcibly
			 * map it.
			 */
			sync_current_stack_to_mm(next);
		}

		/* Stop remote flushes for the previous mm */
		VM_WARN_ON_ONCE(!cpumask_test_cpu(cpu, mm_cpumask(real_prev)) &&
				real_prev != &init_mm);
		cpumask_clear_cpu(cpu, mm_cpumask(real_prev));

		/*
		 * Start remote flushes and then read tlb_gen.
		 */
		cpumask_set_cpu(cpu, mm_cpumask(next));
		next_tlb_gen = atomic64_read(&next->context.tlb_gen);

		// 这里将决定是否需要TLB flush
		choose_new_asid(next, next_tlb_gen, &new_asid, &need_flush);

		/* Let nmi_uaccess_okay() know that we're changing CR3. */
		this_cpu_write(cpu_tlbstate.loaded_mm, LOADED_MM_SWITCHING);
		barrier();

		if (need_flush) {
			this_cpu_write(cpu_tlbstate.ctxs[new_asid].ctx_id, next->context.ctx_id);
			this_cpu_write(cpu_tlbstate.ctxs[new_asid].tlb_gen, next_tlb_gen);
			// x86中的TLB操作属于硬件实现
			// 使用对应pgd加载cr3寄存器进行地址空间切换的时候，硬件会帮你操作TLB
			// 这里true和false就表示是否需要flush
			load_new_mm_cr3(next->pgd, new_asid, true);

			/*
			 * NB: This gets called via leave_mm() in the idle path
			 * where RCU functions differently.  Tracing normally
			 * uses RCU, so we need to use the _rcuidle variant.
			 *
			 * (There is no good reason for this.  The idle code should
			 *  be rearranged to call this before rcu_idle_enter().)
			 */
			trace_tlb_flush_rcuidle(TLB_FLUSH_ON_TASK_SWITCH, TLB_FLUSH_ALL);
		} else {
			/* The new ASID is already up to date. */
			load_new_mm_cr3(next->pgd, new_asid, false);

			/* See above wrt _rcuidle. */
			trace_tlb_flush_rcuidle(TLB_FLUSH_ON_TASK_SWITCH, 0);
		}

		/*
		 * Record last user mm's context id, so we can avoid
		 * flushing branch buffer with IBPB if we switch back
		 * to the same user.
		 */
		if (next != &init_mm)
			this_cpu_write(cpu_tlbstate.last_ctx_id, next->context.ctx_id);

		/* Make sure we write CR3 before loaded_mm. */
		barrier();

		this_cpu_write(cpu_tlbstate.loaded_mm, next);
		this_cpu_write(cpu_tlbstate.loaded_mm_asid, new_asid);
	}

	load_mm_cr4(next);
	switch_ldt(real_prev, next);
}


static void load_new_mm_cr3(pgd_t *pgdir, u16 new_asid, bool need_flush)
{
	unsigned long new_mm_cr3;

	if (need_flush) {
		invalidate_user_asid(new_asid);
		new_mm_cr3 = build_cr3(pgdir, new_asid);
	} else {
		new_mm_cr3 = build_cr3_noflush(pgdir, new_asid);
	}

	/*
	 * Caution: many callers of this function expect
	 * that load_cr3() is serializing and orders TLB
	 * fills with respect to the mm_cpumask writes.
	 */
	write_cr3(new_mm_cr3);
}


// 文件：/arch/x86/include/asm/switch_to.h

// 从上面的流程注释可以看到，这里的流程就是要切换寄存器和栈
//
// 似乎是因为历史原因所以会是宏的形式，可能考虑到传参的影响
//
// prev和next是输入的参数，last为输出（因为是宏所以只能这么干）
// prev：要换走的当前进程
// next：被选中的要换入的下一个进程
// last：切换到当前进程的进程
//
// last是为了得知prev被切换到next，然后经历了漫长的岁月后又从某个task切换回prev
// 而这某个task就是last
#define switch_to(prev, next, last)					\
do {									\
	prepare_switch_to(next);					\
									\
	((last) = __switch_to_asm((prev), (next)));			\
} while (0)




/* This runs runs on the previous thread's stack. */
static inline void prepare_switch_to(struct task_struct *next)
{
#ifdef CONFIG_VMAP_STACK
	/*
	 * If we switch to a stack that has a top-level paging entry
	 * that is not present in the current mm, the resulting #PF will
	 * will be promoted to a double-fault and we'll panic.  Probe
	 * the new stack now so that vmalloc_fault can fix up the page
	 * tables if needed.  This can only happen if we use a stack
	 * in vmap space.
	 *
	 * We assume that the stack is aligned so that it never spans
	 * more than one top-level paging entry.
	 *
	 * To minimize cache pollution, just follow the stack pointer.
	 */
	READ_ONCE(*(unsigned char *)next->thread.sp);
#endif
}


// 文件：/arch/x86/entry/entry_64.S
/*
 * %rdi: prev task
 * %rsi: next task
 */
ENTRY(__switch_to_asm)
	UNWIND_HINT_FUNC
	/*
	 * Save callee-saved registers
	 * This must match the order in inactive_task_frame
	 */
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	/* switch stack */
	// TASK_threadsp这个宏并不好找，大概是task_struct -> thread_struct -> sp
	// https://elixir.bootlin.com/linux/v4.18.20/source/arch/arc/kernel/asm-offsets.c#L20
	// sp指向内核栈
	movq	%rsp, TASK_threadsp(%rdi)
	movq	TASK_threadsp(%rsi), %rsp

#ifdef CONFIG_STACKPROTECTOR
	movq	TASK_stack_canary(%rsi), %rbx
	movq	%rbx, PER_CPU_VAR(irq_stack_union)+stack_canary_offset
#endif

#ifdef CONFIG_RETPOLINE
	/*
	 * When switching from a shallower to a deeper call stack
	 * the RSB may either underflow or use entries populated
	 * with userspace addresses. On CPUs where those concerns
	 * exist, overwrite the RSB with entries which capture
	 * speculative execution to prevent attack.
	 */
	FILL_RETURN_BUFFER %r12, RSB_CLEAR_LOOPS, X86_FEATURE_RSB_CTXSW
#endif

	/* restore callee-saved registers */
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp

	// 这个时候%rdi和%rsi还没有改动，因此传递过去的参数仍是prev和next
	jmp	__switch_to
END(__switch_to_asm)


// 文件：/arch/x86/kernel/process_64.c
/*
 *	switch_to(x,y) should switch tasks from x to y.
 *
 * This could still be optimized:
 * - fold all the options into a flag word and test it with a single test.
 * - could test fs/gs bitsliced
 *
 * Kprobes not supported here. Set the probe on schedule instead.
 * Function graph tracer not supported too.
 */
// 进一步处理寄存器
// 比如段寄存器和FPU
// 还有一些我不知道是啥的寄存器（我太菜了）
//
// thread_struct应该是一个体系结构强相关的类型，从这里获取相关寄存器
__visible __notrace_funcgraph struct task_struct *
__switch_to(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->thread;
	struct thread_struct *next = &next_p->thread;
	struct fpu *prev_fpu = &prev->fpu;
	struct fpu *next_fpu = &next->fpu;
	int cpu = smp_processor_id();
	// TSS用不着了，下面也基本没有代码和它有关
	struct tss_struct *tss = &per_cpu(cpu_tss_rw, cpu);

	WARN_ON_ONCE(IS_ENABLED(CONFIG_DEBUG_ENTRY) &&
		     this_cpu_read(irq_count) != -1);

	switch_fpu_prepare(prev_fpu, cpu);

	// 下面是处理段寄存器
	//
	// TLS实现依赖于%fs和%gs
	// 因此在处理TLS前对%fs和%gs保护现场
	//
	// 处理es ds fs
	//
	// cs和ss在其它地方处理

	/* We must save %fs and %gs before load_TLS() because
	 * %fs and %gs may be cleared by load_TLS().
	 *
	 * (e.g. xen_load_tls())
	 */
	save_fsgs(prev_p);

	/*
	 * Load TLS before restoring any segments so that segment loads
	 * reference the correct GDT entries.
	 */
	load_TLS(next, cpu);

	/*
	 * Leave lazy mode, flushing any hypercalls made here.  This
	 * must be done after loading TLS entries in the GDT but before
	 * loading segments that might reference them, and and it must
	 * be done before fpu__restore(), so the TS bit is up to
	 * date.
	 */
	arch_end_context_switch(next_p);

	/* Switch DS and ES.
	 *
	 * Reading them only returns the selectors, but writing them (if
	 * nonzero) loads the full descriptor from the GDT or LDT.  The
	 * LDT for next is loaded in switch_mm, and the GDT is loaded
	 * above.
	 *
	 * We therefore need to write new values to the segment
	 * registers on every context switch unless both the new and old
	 * values are zero.
	 *
	 * Note that we don't need to do anything for CS and SS, as
	 * those are saved and restored as part of pt_regs.
	 */
	savesegment(es, prev->es);
	if (unlikely(next->es | prev->es))
		loadsegment(es, next->es);

	savesegment(ds, prev->ds);
	if (unlikely(next->ds | prev->ds))
		loadsegment(ds, next->ds);

	load_seg_legacy(prev->fsindex, prev->fsbase,
			next->fsindex, next->fsbase, FS);
	load_seg_legacy(prev->gsindex, prev->gsbase,
			next->gsindex, next->gsbase, GS);

	switch_fpu_finish(next_fpu, cpu);

	/*
	 * Switch the PDA and FPU contexts.
	 */
	// Question. PDA是什么？
	this_cpu_write(current_task, next_p);
	this_cpu_write(cpu_current_top_of_stack, task_top_of_stack(next_p));

	/* Reload sp0. */
	update_sp0(next_p);

	/*
	 * Now maybe reload the debug registers and handle I/O bitmaps
	 */
	if (unlikely(task_thread_info(next_p)->flags & _TIF_WORK_CTXSW_NEXT ||
		     task_thread_info(prev_p)->flags & _TIF_WORK_CTXSW_PREV))
		__switch_to_xtra(prev_p, next_p, tss);

#ifdef CONFIG_XEN_PV
	/*
	 * On Xen PV, IOPL bits in pt_regs->flags have no effect, and
	 * current_pt_regs()->flags may not match the current task's
	 * intended IOPL.  We need to switch it manually.
	 */
	if (unlikely(static_cpu_has(X86_FEATURE_XENPV) &&
		     prev->iopl != next->iopl))
		xen_set_iopl_mask(next->iopl);
#endif

	if (static_cpu_has_bug(X86_BUG_SYSRET_SS_ATTRS)) {
		/*
		 * AMD CPUs have a misfeature: SYSRET sets the SS selector but
		 * does not update the cached descriptor.  As a result, if we
		 * do SYSRET while SS is NULL, we'll end up in user mode with
		 * SS apparently equal to __USER_DS but actually unusable.
		 *
		 * The straightforward workaround would be to fix it up just
		 * before SYSRET, but that would slow down the system call
		 * fast paths.  Instead, we ensure that SS is never NULL in
		 * system call context.  We do this by replacing NULL SS
		 * selectors at every context switch.  SYSCALL sets up a valid
		 * SS, so the only way to get NULL is to re-enter the kernel
		 * from CPL 3 through an interrupt.  Since that can't happen
		 * in the same task as a running syscall, we are guaranteed to
		 * context switch between every interrupt vector entry and a
		 * subsequent SYSRET.
		 *
		 * We read SS first because SS reads are much faster than
		 * writes.  Out of caution, we force SS to __KERNEL_DS even if
		 * it previously had a different non-NULL value.
		 */
		unsigned short ss_sel;
		savesegment(ss, ss_sel);
		if (ss_sel != __KERNEL_DS)
			loadsegment(ss, __KERNEL_DS);
	}

	/* Load the Intel cache allocation PQR MSR. */
	intel_rdt_sched_in();

	return prev_p;
}


// 文件：/kernel/sched/core.c
/**
 * finish_task_switch - clean up after a task-switch
 * @prev: the thread we just switched away from.
 *
 * finish_task_switch must be called after the context switch, paired
 * with a prepare_task_switch call before the context switch.
 * finish_task_switch will reconcile locking set up by prepare_task_switch,
 * and do any other architecture-specific cleanup actions.
 *
 * Note that we may have delayed dropping an mm in context_switch(). If
 * so, we finish that here outside of the runqueue lock. (Doing it
 * with the lock held can cause deadlocks; see schedule() for
 * details.)
 *
 * The context switch have flipped the stack from under us and restored the
 * local variables which were saved when this task called schedule() in the
 * past. prev == current is still correct but we need to recalculate this_rq
 * because prev may have moved to another CPU.
 */
static struct rq *finish_task_switch(struct task_struct *prev)
	__releases(rq->lock)
{
	struct rq *rq = this_rq();
	struct mm_struct *mm = rq->prev_mm;
	long prev_state;

	/*
	 * The previous task will have left us with a preempt_count of 2
	 * because it left us after:
	 *
	 *	schedule()
	 *	  preempt_disable();			// 1
	 *	  __schedule()
	 *	    raw_spin_lock_irq(&rq->lock)	// 2
	 *
	 * Also, see FORK_PREEMPT_COUNT.
	 */
	if (WARN_ONCE(preempt_count() != 2*PREEMPT_DISABLE_OFFSET,
		      "corrupted preempt_count: %s/%d/0x%x\n",
		      current->comm, current->pid, preempt_count()))
		preempt_count_set(FORK_PREEMPT_COUNT);

	rq->prev_mm = NULL;

	/*
	 * A task struct has one reference for the use as "current".
	 * If a task dies, then it sets TASK_DEAD in tsk->state and calls
	 * schedule one last time. The schedule call will never return, and
	 * the scheduled task must drop that reference.
	 *
	 * We must observe prev->state before clearing prev->on_cpu (in
	 * finish_task), otherwise a concurrent wakeup can get prev
	 * running on another CPU and we could rave with its RUNNING -> DEAD
	 * transition, resulting in a double drop.
	 */
	prev_state = prev->state;
	vtime_task_switch(prev);
	perf_event_task_sched_in(prev, current);
	finish_task(prev);
	finish_lock_switch(rq);
	finish_arch_post_lock_switch();
	kcov_finish_switch(current);

	fire_sched_in_preempt_notifiers(current);
	/*
	 * When switching through a kernel thread, the loop in
	 * membarrier_{private,global}_expedited() may have observed that
	 * kernel thread and not issued an IPI. It is therefore possible to
	 * schedule between user->kernel->user threads without passing though
	 * switch_mm(). Membarrier requires a barrier after storing to
	 * rq->curr, before returning to userspace, so provide them here:
	 *
	 * - a full memory barrier for {PRIVATE,GLOBAL}_EXPEDITED, implicitly
	 *   provided by mmdrop(),
	 * - a sync_core for SYNC_CORE.
	 */
	if (mm) {
		membarrier_mm_sync_core_before_usermode(mm);
		mmdrop(mm);
	}
	if (unlikely(prev_state == TASK_DEAD)) {
		if (prev->sched_class->task_dead)
			prev->sched_class->task_dead(prev);

		/*
		 * Remove function-return probe instances associated with this
		 * task and put them back on the free list.
		 */
		kprobe_flush_task(prev);

		/* Task is done with its stack. */
		put_task_stack(prev);

		put_task_struct(prev);
	}

	tick_nohz_task_switch();
	return rq;
}
