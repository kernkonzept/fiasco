/*
 * This file contains a 'gate_init' initialization table
 * to initialize the x86 processor trap vectors to default entrypoints.
 * These entrypoints simply push a standard trap_state frame
 * and jump to the 'trap_handler' routine.
 */

#include "config_tcbsize.h"
#include "config_gdt.h"
#include "globalconfig.h"
#include "idt_init.h"
#include <low_level.h>
#include "shortcut.h"
#include "tcboffset.h"

#include "asm.h"


/* We make the trap handler an interrupt gate, because for debugging
   purposes, we don't want any interrupts to occur until they're
   explicitly enabled in the base_trap_handler (usually
   Thread::handle_slow_trap). */

/*
 * No error code.  Clear error code and push trap number.
 */
.macro	EXCEPTION n, name
	GATE_ENTRY	\n, entry_\name, (ACC_PL_K | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	SWITCH_TO_KERNEL_CR3 0
	push	$(0)
	push	$(\n)
	save_all_regs
	jmp	_slowtraps
.endm

/*
 * User-accessible exception.  Otherwise, same as above.
 */
.macro	EXCEP_USR n, name
	GATE_ENTRY	\n, entry_\name, (ACC_PL_U | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	SWITCH_TO_KERNEL_CR3 0
	push	$(0)
	push	$(\n)
	save_all_regs
	jmp	_slowtraps
.endm

/*
 * Error code has been pushed.  Just push trap number.
 */
.macro	EXCEP_ERR n, name, from_iret_fault = 0
	GATE_ENTRY	\n, entry_\name, (ACC_PL_K | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	SWITCH_TO_KERNEL_CR3 1, \from_iret_fault
	push	$(\n)
	save_all_regs
	jmp	_slowtraps
.endm

GATE_INITTAB_BEGIN idt_init_table

	.section ".entry.text.vec", "ax", @progbits

EXCEPTION	0x00, vec00_zero_div
/* IA32 has to handle breakpoint exceptions if occurred exactly at
   entry_sys_fast_ipc -- see ia32/entry-ia32.S */
GATE_ENTRY	0x01, entry_vec01_debug, (ACC_PL_K | ACC_INTR_GATE)
/* XXX IA32 has to handle NMI occurred exactly at entry_sys_fast_ipc */
EXCEPTION	0x02, vec02_nmi
EXCEP_USR	0x03, vec03_breakpoint
EXCEPTION	0x04, vec04_into
EXCEPTION	0x05, vec05_bounds
EXCEPTION	0x06, vec06_invop
# EXCEPTION	0x07, nofpu
// XXX we can't use a task gate, instead we must use IST
GATE_ENTRY	0x08, entry_vec08_dbf, (ACC_PL_K | ACC_INTR_GATE)
EXCEPTION	0x09, vec09_fpu_ovfl
# EXCEP_ERR	0x0a, vec0a_inv_tss
EXCEP_ERR	0x0b, vec0b_segnp, 1
EXCEP_ERR	0x0c, vec0c_stack_fault, 1
EXCEP_ERR	0x0d, vec0d_gen_prot, 1
# EXCEP_ERR	0x0e, vec0e_page_fault
# EXCEPTION	0x0f, vec0f_trap_0f
EXCEPTION	0x10, vec10_fpu_err
EXCEP_ERR	0x11, vec11_align
EXCEPTION	0x12, vec12_mcheck
EXCEPTION	0x13, vec13_simd_err

	.section ".entry.text.slowtrap", "ax", @progbits
	.p2align 4
	.type	slowtraps,@function
	.globl	slowtraps
	/* We have to introduce the label _slowtraps besides the label
	   slowtraps to achieve that jmps from exception entry points
	   are optimized to two-byte jmps. The label slowtraps is visible
	   from outside. */
_slowtraps:
slowtraps:
	IA32_IBRS
.Lenter_slowtrap_after_fixup:
	mov	%rsp,%rdi		/* ARG1: address of trap_state */
	mov	$0, %esi		/* ARG2: default CPU = 0 */
	cld

        /* Call the C handler function if one has been installed.  */
	mov	BASE_TRAP_HANDLER, %rax
	or	%rax,%rax
	jz	.Lunexpected_trap
	call	*%rax
in_slowtrap:

	/* If the handler function returned zero (success),
	   then resume execution as if the trap never happened.
	   Otherwise, just panic.  */
	or	%rax,%rax
	jnz	.Lunexpected_trap

	restore_all_regs
	add	$0x10,%rsp		/* pop trap number and error code */
	SAFE_IRET

.align 8
.global iretq_exception_handler
iretq_exception_handler:
	/* patch return address from base-trap handler to the iret fixup */
	movq	$.Liretq_exception_stack_fix, -8(%rsi)
	mov	$1, %eax
	retq

.align 8
.Liretq_exception_stack_fix:
        /* The actual stack frame is 6 words below the original
           stack frame, because of 16byte alignment of AMD64 SP
           on kernel entry, and an odd length of the Trap_state
           structure */
	mov	$18, %ecx
	lea	(17 * 8)(%rsp), %rsi
	lea	(23 * 8)(%rsp), %rdi
	std
	rep movsq	%ds:(%rsi), %es:(%rdi)
	cld
	add	$(6 * 8), %rsp
	jmp	.Lenter_slowtrap_after_fixup

.align 8
.Lunexpected_trap:
	mov	%rsp,%rdi		/* 1st arg: trap state */
	call	trap_dump_panic

	.section ".entry.text.vec0e", "ax", @progbits
GATE_ENTRY	0x0e, entry_vec0e_page_fault, (ACC_PL_K | ACC_INTR_GATE)

/* we must save %cr2 before we can be preempted -- therefore we're an
   interrupt gate (invoked with interrupts turned off).  Also, we
   don't turn them on again here, but only after checking for
   page-ins from the global page directory in thread_page_fault().
   XXX: If you make changes to stack layout here, fix thread_page_fault */

/* XXX slow version - sets up nice stack frame for debugger */

	.p2align 4
	.type	entry_vec0e_page_fault,@function
.globl entry_vec0e_page_fault
entry_vec0e_page_fault:
	cld
	SWITCH_TO_KERNEL_CR3	1
	SAVE_SCRATCH
	IA32_IBRS

/* We must reset the cancel flag here atomically
   if we are entering fresh from user mode and an IPC might occur.
   NOTE: We cannot test the user-mode bit in the error code because
   it will flag "kernel" in case an I/O-bitmap page is not mapped
   during an I/O access. */

	mov     (SCRATCH_REGISTER_SIZE + 0x10)(%rsp),%rcx   /* get CS from stack */
        andb    $3,%cl		/* retrieve current privilege level (CPL) */
	jz	1f		/* CPL == 0 -> kernel, skip resetting state */
	ESP_TO_TCB_AT %rcx
	RESET_THREAD_CANCEL_AT %rcx
1:
	mov     PAGE_FAULT_ADDR,%rdi	/* arg1: page fault address */
	pushq   %rdi			/* save pfa on stack as well */
	leaq	(SCRATCH_REGISTER_SIZE + 0x10)(%rsp),%r8	/* arg5: ptr to return frame */
	mov     (SCRATCH_REGISTER_SIZE + 0x08)(%rsp),%rsi	/* arg2: error code */
	mov	(SCRATCH_REGISTER_SIZE + 0x10)(%rsp),%rdx	/* arg3: rip */
	mov	(SCRATCH_REGISTER_SIZE + 0x20)(%rsp),%rcx	/* arg4: rflags */
	call	thread_page_fault

in_page_fault:
	or	%rax,%rax
	jz	.Lbad_page_fault
	add	$8, %rsp			/* drop pfa */
	RESTORE_SCRATCH
	add	$8,%rsp				/* remove error code */
	SAFE_IRET

/* If code or stack from a small address space are not yet mapped in the
   current page directory we might get a page fault on return from the
   trampoline page. In this case we cannot return to the trampoline page
   after handling the fault because we are already in user mode (with
   segment limits below kernel space) while the trampoline code is located
   in kernel data space. So instead we change ESP and EIP to point to the
   address the trampoline wanted to return to and do the normal IRET. */

/* recover from a bad page fault by invoking the slow_trap handler */
	.p2align 4
.Lbad_page_fault:
	cli
	/* we have on stack: r8, rdi, rsi, rdx, rcx, rax, error code
	   move registers down to make room for trap number 
	   and build complete trap state before  jumping to trap handler */
	popq	%rdi		/* pop pfa */
	mov	%rdi, %cr2	/* restore pfa */
	RESTORE_SCRATCH
	pushq	$0x0e
	save_all_regs
	jmp	slowtraps

	.section ".entry.text.vec07", "ax", @progbits
/* FPU not available in this context. */
GATE_ENTRY	0x07, entry_vec07_fpu_unavail, (ACC_PL_K | ACC_INTR_GATE)

/* do all of this with disabled interrupts */
	.p2align 4
	.type	entry_vec07_fpu_unavail,@function
entry_vec07_fpu_unavail:
	cld
	SWITCH_TO_KERNEL_CR3 0
	SAVE_SCRATCH
	mov 	SCRATCH_REGISTER_SIZE(%rsp), %rdi
	call	thread_handle_fputrap
in_handle_fputrap:
	test 	%eax,%eax
	jz	real_fpu_exception
	RESTORE_SCRATCH
	SAFE_IRET
real_fpu_exception:
	RESTORE_SCRATCH
	pushq	$(0)
	pushq	$(7)
	save_all_regs
	jmp	_slowtraps

	.section ".entry.text.timer", "ax", @progbits
/* timer interrupt */
#ifdef CONFIG_SCHED_PIT
GATE_ENTRY	0x20, entry_int_timer, (ACC_PL_K | ACC_INTR_GATE)
#endif
#ifdef CONFIG_SCHED_RTC
GATE_ENTRY	0x28, entry_int_timer, (ACC_PL_K | ACC_INTR_GATE)
#endif
#ifdef CONFIG_SCHED_APIC
GATE_ENTRY	APIC_IRQ_BASE, entry_int_timer, (ACC_PL_K | ACC_INTR_GATE)
#endif
#ifdef CONFIG_SCHED_HPET
/* HPET is set at startup */
#endif

	.p2align 4
	.globl	entry_int_timer
entry_int_timer:
	SWITCH_TO_KERNEL_CR3	0
	SAVE_SCRATCH
	IA32_IBRS
do_timer_interrupt:
	cld
	mov	SCRATCH_REGISTER_SIZE(%rsp),%rdi /* pass rip for logging */
	call	thread_timer_interrupt		/* enter with disabled irqs */
in_timer_interrupt:
	RESTORE_SCRATCH
	SAFE_IRET

	.p2align 4
	.globl	entry_int_timer_slow
entry_int_timer_slow:
	cld
	SWITCH_TO_KERNEL_CR3	0
	SAVE_SCRATCH
	IA32_IBRS
	call	thread_timer_interrupt_slow	/* enter with disabled irqs */
in_timer_interrupt_slow:
	jmp	do_timer_interrupt

	
	.p2align 4
	.globl	entry_int_timer_stop
entry_int_timer_stop:
	cld
	SWITCH_TO_KERNEL_CR3	0
	SAVE_SCRATCH
	call	thread_timer_interrupt_stop
	RESTORE_SCRATCH
	SAFE_IRET

/* other interrupts */

.macro	INTERRUPT int, name
	GATE_ENTRY	\int, entry_\name, (ACC_PL_K | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	SWITCH_TO_KERNEL_CR3 0
	SAVE_SCRATCH
	mov	0x28(%rsp), %rsi
	mov	$ (\int - 0x20), %rdi
	jmp	all_irqs
.endm

/**
 * Stub array for all IRQ vector entries in the IDT
 */

#ifdef CONFIG_KERNEL_NX

/*
 * Note that the array is split into the data part and the code part because
 * the kernel write-protects its code and prevents execution of its data.
 */

STUB_DATA_SIZE = 8
STUB_TEXT_SIZE = 16

	.section ".entry.data.irqs", "aw", @progbits
	.global idt_irq_vector_stubs
	.global idt_irq_vector_stubs_data
	.align 8
idt_irq_vector_stubs:
idt_irq_vector_stubs_data:
.rept APIC_IRQ_BASE - 0x28
	.quad 0
.endr

	.section ".entry.text.irqs", "ax", @progbits
	.global idt_irq_vector_stubs_text
	.align 64
idt_irq_vector_stubs_text:
.rept APIC_IRQ_BASE - 0x28
0:
	push	%rdi
	movq	(idt_irq_vector_stubs_data + STUB_DATA_SIZE * (0b - idt_irq_vector_stubs_text) / STUB_TEXT_SIZE), %rdi
	jmp	__generic_irq_entry
	/* the entries are not constant length, align to compensate */
	.align 16
.endr

#else /* CONFIG_KERNEL_NX */

/*
 * The immediate in the movabs shall be patched with the
 * address of an Irq_base object that is later attached to
 * the corresponding IDT vector.
 */
	.section ".entry.text.irqs", "ax", @progbits
	.global idt_irq_vector_stubs
	.align 64
idt_irq_vector_stubs:
.rept APIC_IRQ_BASE - 0x28
	push	%rdi
	movabs	$0, %rdi
	jmp	__generic_irq_entry
.endr

#endif /* CONFIG_KERNEL_NX */


	.type __generic_irq_entry,@function
	.global __generic_irq_entry

__generic_irq_entry:
	SWITCH_TO_KERNEL_CR3 1
	push    %rsi
	push	%rax
	push	%rcx
	push	%rdx
	push	%r8
	push	%r9
	push	%r10
	push	%r11

	.p2align 4
	.type	all_irqs,@function
all_irqs:
	IA32_IBRS
	cld
	call	irq_interrupt			/* enter with disabled irqs */
in_interrupt:
	RESTORE_SCRATCH

entry_int_pic_ignore:
	SAFE_IRET

	.global	entry_int_pic_ignore
	.global	entry_int7
	.global	entry_intf

#ifndef CONFIG_SCHED_PIT
INTERRUPT	0x20, int0
#endif
INTERRUPT	0x27, int7
INTERRUPT	0x2f, intf
#ifndef CONFIG_SCHED_RTC
INTERRUPT	0x28, int8
#endif

/* system calls */

#ifndef CONFIG_KERNEL_ISOLATION
#ifdef CONFIG_MP
MAX_NUM_CPUS = CONFIG_MP_MAX_CPUS
#else
MAX_NUM_CPUS = 1
#endif

#define SYSCALL_ENTRY_DATA_SIZE 64
#define SYSCALL_ENTRY_TEXT_SIZE (0f - 0b)
#define SYSCALL_ENTRY_OFFSET ((0b - syscall_entry_text) / SYSCALL_ENTRY_TEXT_SIZE)
#define SYSCALL_ENTRY_DATA (syscall_entry_data + SYSCALL_ENTRY_OFFSET * SYSCALL_ENTRY_DATA_SIZE)
#define KERN_SP (SYSCALL_ENTRY_DATA + 0)
#define USER_SP (SYSCALL_ENTRY_DATA + 8)
	.section ".entry.text.syscalls", "ax", @progbits
	.global syscall_entry_text
	.align 64
syscall_entry_text:
.rept MAX_NUM_CPUS
0:
	mov %rsp, USER_SP(%rip)
	mov KERN_SP(%rip), %rsp
	mov (%rsp), %rsp
	pushq $GDT_DATA_USER | 3
	pushq USER_SP(%rip)
	jmp entry_sys_fast_ipc_c
	.align 32
0:
.endr
#endif /* !CONFIG_KERNEL_ISOLATION */

        // these labels help show_tcb to guess the thread state
	.globl	in_slowtrap
	.globl	in_page_fault
	.globl	in_handle_fputrap
	.globl	in_interrupt  
	.globl	in_timer_interrupt
	.globl	in_timer_interrupt_slow


/* these functions are implemented in entry-native.S */
GATE_ENTRY	0x0a, entry_vec0a_invalid_tss, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	0x0f, entry_vec0f_apic_spurious_interrupt_bug, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE + 3), entry_apic_error_interrupt, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE + 4), entry_apic_spurious_interrupt, (ACC_PL_K | ACC_INTR_GATE)

#ifdef CONFIG_MP
GATE_ENTRY	(APIC_IRQ_BASE + 2), entry_ipi, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE - 2), entry_debug_ipi, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE - 1), entry_ipi_remote_request, (ACC_PL_K | ACC_INTR_GATE)
#endif

GATE_INITTAB_END

	.section ".entry.text.upcalls", "ax", @progbits
	.p2align 3
	.globl	leave_by_trigger_exception
leave_by_trigger_exception:
        sub $40, %rsp
	pushq	$0x00
	pushq	$0xff
	save_all_regs
	pushq	$_slowtraps
	jmp	thread_restore_exc_state


#define OFS__VCPU_STATE__REGS_IRET (VAL__SIZEOF_TRAP_STATE - 40 + OFS__VCPU_STATE__TREX)

	.p2align 3
	.globl	leave_by_vcpu_upcall
leave_by_vcpu_upcall:
	sub	$40,%rsp		/* clean up stack from previous
					 * CPL0-CPL0 iret */
	SAVE_SCRATCH
	call	thread_restore_exc_state

	ESP_TO_TCB_AT %rcx
	mov OFS__THREAD__USER_VCPU(%rcx), %rdi
	mov OFS__THREAD__VCPU_STATE(%rcx), %rcx
	add $(OFS__VCPU_STATE__REGS_IRET), %rcx
	mov SCRATCH_REGISTER_SIZE(%rsp),  %rdx
	mov %rdx, (%rcx)                            /* RIP */
	mov 8 + SCRATCH_REGISTER_SIZE(%rsp),  %rdx
	mov %rdx, 8(%rcx)                            /* CS */
	mov 16 + SCRATCH_REGISTER_SIZE(%rsp),  %rdx
	mov %rdx, 16(%rcx)                          /* RFLAGS */
	mov 24 + SCRATCH_REGISTER_SIZE(%rsp), %rdx
	mov %rdx, 24(%rcx)                          /* RSP */
	mov 32 + SCRATCH_REGISTER_SIZE(%rsp), %rdx
	mov %rdx, 32(%rcx)                          /* SS */

	mov (0*8)(%rsp), %rdx   /* r11 */
	mov %rdx, -(14*8)(%rcx)
	mov (1*8)(%rsp), %rdx   /* r10 */
	mov %rdx, -(13*8)(%rcx)
	mov (2*8)(%rsp), %rdx
	mov %rdx, -(12*8)(%rcx) /* r9 */
	mov (3*8)(%rsp), %rdx
	mov %rdx, -(11*8)(%rcx) /* r8 */
	mov (4*8)(%rsp), %rdx
	mov %rdx, -(5*8)(%rcx)  /* rdx */
	mov (5*8)(%rsp), %rdx
	mov %rdx, -(4*8)(%rcx)  /* rcx */
	mov (6*8)(%rsp), %rdx
	mov %rdx, -(3*8)(%rcx)  /* rax */
	mov (7*8)(%rsp), %rdx
	mov %rdx, -(9*8)(%rcx)  /* rsi */
	mov (8*8)(%rsp), %rdx
	mov %rdx, -(10*8)(%rcx) /* rdi */

	lea SCRATCH_REGISTER_SIZE(%rsp), %rdx
	lea -(5*8)(%rcx), %rsp

	push %rbx
	sub $8, %rsp
	push %rbp
	sub $(6*8), %rsp
	push %r12
	push %r13
	push %r14
	push %r15

	/*add SCRATCH_REGISTER_SIZE, %esp*/
	mov %rdx, %rsp
	mov -OFS__VCPU_STATE__REGS_IRET + OFS__VCPU_STATE__ENTRY_SP(%rcx), %rax
	mov %rax, 24(%rsp)
	mov -OFS__VCPU_STATE__REGS_IRET + OFS__VCPU_STATE__ENTRY_IP(%rcx), %rax
	mov %rax, 0(%rsp)
	movq $EFLAGS_IF, 16(%rsp)
        movq $(GDT_CODE_USER | SEL_PL_U), 8(%rsp)
	SAFE_IRET

	.p2align 3
	.globl	vcpu_resume
vcpu_resume:
	mov %rdi, %rsp
#if 0
	popq %es
	popq %ds
	popq REG_GS
	popq %fs
#endif
	restore_all_regs
	add $(2*8), %rsp
	andq	$0x7f, 8(%rsp)			// if entered using syscall
	SAFE_IRET

        .p2align 3
        .globl  leave_and_kill_myself
leave_and_kill_myself:
        // make space for a dummy Return_frame accessible by the callee
        subq    $(5*8),%rsp                     // sizeof(Return_frame)
        call    thread_do_leave_and_kill_myself
        // does never return

