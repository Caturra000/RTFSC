// 文件：/arch/x86/include/asm/irq_vectors.h

/*
 * Linux IRQ vector layout.
 *
 * There are 256 IDT entries (per CPU - each entry is 8 bytes) which can
 * be defined by Linux. They are used as a jump table by the CPU when a
 * given vector is triggered - by a CPU-external, CPU-internal or
 * software-triggered event.
 *
 * Linux sets the kernel code address each entry jumps to early during
 * bootup, and never changes them. This is the general layout of the
 * IDT entries:
 *
 *  Vectors   0 ...  31 : system traps and exceptions - hardcoded events
 *  Vectors  32 ... 127 : device interrupts
 *  Vector  128         : legacy int80 syscall interface
 *  Vectors 129 ... INVALIDATE_TLB_VECTOR_START-1 except 204 : device interrupts
 *  Vectors INVALIDATE_TLB_VECTOR_START ... 255 : special interrupts
 *
 * 64-bit x86 has per CPU IDT tables, 32-bit has one shared IDT table.
 *
 * This file enumerates the exact layout of them:
 */

// Intel SDM Vol.3提到，x86体系下interrupt是不归类到exception里面的，
// 然而迷惑的是0-31号vector中有部分就是interrupt
// 更迷惑的是enum全部用X86_TRAP前缀来命名，明明里面不仅有interrupt，还有其它类型的exception比如fault
// 既然手册都这么随便了，那就算了吧，不纠结了
