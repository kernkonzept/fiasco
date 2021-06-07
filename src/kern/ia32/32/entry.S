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
#include "regdefs.h"

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
	pushl	$(0)
	pushl	$(\n)
	pusha
	jmp	.L_slowtraps
.endm

/*
 * User-accessible exception.  Otherwise, same as above.
 */
.macro	EXCEP_USR n,name
	GATE_ENTRY	\n, entry_\name, (ACC_PL_U | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	pushl	$(0)
	pushl	$(\n)
	pusha
	jmp	.L_slowtraps
.endm

/*
 * Error code has been pushed.  Just push trap number.
 */
.macro	EXCEP_ERR n, name
	GATE_ENTRY	\n, entry_\name, (ACC_PL_K | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	pushl	$(\n)
	pusha
	jmp	.L_slowtraps
.endm

.macro  PRE_ALIEN_IPC
	btrl	$17, OFS__THREAD__STATE (%ebx) /* Thread_dis_alien */
	jc	1f
	RESTORE_STATE
	subl	$2, 4(%esp)                /* Correct EIP to point to insn */
	popl 	%eax
	pushl	$(0x30 << 3 | 2)
	pushl	$(0xd)
	pusha
	jmp	.L_slowtraps

1: 	/* do alien IPC and raise a trap afterwards */	
	RESET_THREAD_CANCEL_AT %ebx
.endm

.macro  POST_ALIEN_IPC
	CHECK_SANITY $3			/* scratches ecx */
	RESTORE_STATE_AFTER_IPC
	popl	%eax
	pushl	$(0x30 << 3 | 6)
	pushl	$(0xd)
	pusha
	jmp	.L_slowtraps
.endm

GATE_INITTAB_BEGIN idt_init_table

EXCEPTION	0x00, vec00_zero_div
#ifdef CONFIG_PF_UX
EXCEPTION	0x01, vec01_debug
#else
/* IA32 has to handle breakpoint exceptions if occurred exactly at
   entry_sys_fast_ipc -- see ia32/entry-ia32.S */
GATE_ENTRY	0x01, entry_vec01_debug, (ACC_PL_K | ACC_INTR_GATE)
#endif
/* XXX IA32 has to handle NMI occurred exactly at entry_sys_fast_ipc */
EXCEPTION	0x02, vec02_nmi
EXCEP_USR	0x03, vec03_breakpoint
EXCEPTION	0x04, vec04_into
EXCEPTION	0x05, vec05_bounds
EXCEPTION	0x06, vec06_invop
# EXCEPTION	0x07, nofpu
#ifdef CONFIG_PF_UX
EXCEP_ERR	0x08, vec08_dbl_fault
#else
GATE_ENTRY	0x08, GDT_TSS_DBF, (ACC_PL_K | ACC_TASK_GATE)
#endif
EXCEPTION	0x09, vec09_fpu_ovfl
# EXCEP_ERR	0x0a, vec0a_inv_tss
EXCEP_ERR	0x0b, vec0b_segnp
EXCEP_ERR	0x0c, vec0c_stack_fault
EXCEP_ERR	0x0d, vec0d_gen_prot
# EXCEP_ERR	0x0e, vec0e_page_fault
# EXCEPTION	0x0f, vec0f_trap_0f
EXCEPTION	0x10, vec10_fpu_err
EXCEP_ERR	0x11, vec11_align
EXCEPTION	0x12, vec12_mcheck
EXCEPTION	0x13, vec13_simd_err


	.p2align 4
	.type	slowtraps,@function
	.globl	slowtraps
	/* We have to introduce the label .L_slowtraps besides the label
	   slowtraps to achieve that jmps from exception entry points
	   are optimized to two-byte jmps. The label slowtraps is visible
	   from outside. */
.L_slowtraps:
slowtraps:
#ifdef CONFIG_PF_UX
# define REG_GS CPU_GS
# define REG_FS CPU_FS
#else
# define REG_GS %gs
# define REG_FS %fs
#endif

	pushl	REG_FS		/* we save the segment regs in the trap   */
.Lslowtrap_push_gs:
	pushl	REG_GS		/* state, but we do not restore them.  We */
.Lslowtrap_push_ds:
	pushl	%ds		/* rather reload them using               */
	pushl	%es		/* RESET_{KERNEL,USER}_SEGMENTS           */

	cld

	/* Load the kernel's segment registers.  */
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES /* scratches ecx, edx */
	
	/* Note: we do not use RESET_THREAD_CANCEL_* here as that is
	   needed only when an I/O-page-fault IPC is sent and when the
	   thread is killed. Resetting Thread_cancel here could
	   be harmful when using this trap handler in debugging.
	   Instead, we clear this flag in Thread::handle_slow_trap()
	   just before sending the IPC message or before killing the
	   thread.  That's OK, because it is still atomic -- we never
	   enable IRQs (sti) before that point. */

.Lenter_slowtrap_after_fixup:
	movl	%esp,%eax		/* ARG1: address of trap_state */
	xorl	%edx,%edx		/* ARG2: default CPU = 0 */

        /* Call the C handler function if one has been installed.  */
	movl	BASE_TRAP_HANDLER, %ecx
	orl	%ecx,%ecx
	jz	.Lunexpected_trap
	call	*%ecx
in_slowtrap:

	/* If the handler function returned zero (success),
	   then resume execution as if the trap never happened.
	   Otherwise, just panic.  */
	orl	%eax,%eax
	jnz	.Lunexpected_trap

	CHECK_SANITY 60(%esp)		/* scratches ecx */

	addl	$4*2,%esp		/* Pop ds, es segment registers */
1:	popl REG_GS
2:	popl REG_FS
	popa
	addl	$4*2,%esp		/* Pop trap number and error code */
3:	iret

	/* set fixups for segment pops */
	ASM_KEX 1b, 11f
	ASM_KEX 2b, 22f
	ASM_KEX 3b, 0, .Liret_exception_handler

	.section .fixup, "ax?", @progbits
	.align 4
11:	movl	$13, (10 * 4)(%esp) /* GP as trap number */
	mov	%gs, (11 * 4)(%esp) /* segment selector as error code */
	jmp .Lslowtrap_push_ds

	.align 4
22:	movl	$13, ( 9 * 4)(%esp) /* GP as trap number */
	mov	%fs, (10 * 4)(%esp) /* segment selector as error code */
	jmp .Lslowtrap_push_gs

	/* write new return address below trap state.
	 * So when we return from the slowtrap handlers
	 * we can fix the extra stack offset from the
	 * original stack frame */
	.align 4
.Liret_exception_handler:
	movl	$.Liret_exception_stack_fix, -4(%edx)
	mov	$1, %eax
	ret

	.align 4
.Liret_exception_stack_fix:
	mov	$14, %ecx
	lea	(13 * 4)(%esp), %esi
	lea	(16 * 4)(%esp), %edi
	std
	rep movsl	%ds:(%esi), %es:(%edi)
	cld
	add	$(3 * 4), %esp
	jmp	.Lenter_slowtrap_after_fixup
	.previous

	.align 4
.Lunexpected_trap:
	movw	%ss,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movl	%esp,%eax
	call	trap_dump_panic

GATE_ENTRY	0x0e, entry_vec0e_page_fault, (ACC_PL_K | ACC_INTR_GATE)

/* we must save %cr2 before we can be preempted -- therefore we're an
   interrupt gate (invoked with interrupts turned off).  Also, we
   don't turn them on again here, but only after checking for
   page-ins from the global page directory in thread_page_fault().
   XXX: If you make changes to stack layout here, fix thread_page_fault */

/* XXX slow version - sets up nice stack frame for debugger */

	.p2align 4
	.type	entry_vec0e_page_fault,@function
entry_vec0e_page_fault:
	cld
	SAVE_SCRATCH
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	movl    12(%esp),%edx		/* save error code in ARG2 ... */
	movl    PAGE_FAULT_ADDR,%eax	/* save page fault address in ARG1 */

/* We must reset the cancel flag here atomically 
   if we are entering fresh from user mode and an IPC might occur. 
   NOTE: We cannot test the user-mode bit in the error code because
   it will flag "kernel" in case an I/O-bitmap page is not mapped
   during an I/O access. */

	movl    20(%esp),%ecx   /* get CS from stack */
        andb    $3,%cl		/* retrieve current privilege level (CPL) */
	jz	1f		/* CPL == 0 -> kernel, skip resetting state */
	ESP_TO_TCB_AT %ecx
	RESET_THREAD_CANCEL_AT %ecx
1:	movl	%ebp,12(%esp)	/* save frame pointer */
	leal    12(%esp),%ebp	/* load new frame pointer */
	pushl	%eax		/* save pf address */
	pushl	%edx		/* save error code */
	leal	24(%esp),%ecx	/* ARG5: ptr to Return_frame */
	pushl	%ecx
	pushl	36(%esp)	/* ARG4: eflags */
	movl	32(%esp),%ecx	/* ARG3: eip */
	call	thread_page_fault
in_page_fault:

	orl	%eax,%eax
	jz	.Lbad_page_fault
	lea	16(%esp),%esp
	CHECK_SANITY 20(%esp)			/* scratches ecx */
	RESTORE_SCRATCH
	popl	%ebp
	iret

/* recover from a bad page fault by invoking the slow_trap handler */
	.p2align 4
.Lbad_page_fault:
	cli
	addl	$8,%esp		/* pop ARG4 and ARG5 */
	movl	(%ebp),%eax	/* old ebp */
	popl	(%ebp)		/* error code */
	popl	%edx		/* page fault address */
	movl	%eax,%ebp

	/* we have on stack: error code, eax, ecx, edx
	   move registers down to make room for trap number */
	subl	$4,%esp
	movl	4(%esp),%eax
	movl	%eax,(%esp)
	movl	8(%esp),%eax
	movl	%eax,4(%esp)
	movl	12(%esp),%eax
	movl	%eax,8(%esp)
	movl	$0x0e,12(%esp)	/* error code */
	pushl	%ebx		/* rest of trap state */
	pushl	%edx		/* page fault address */
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	jmp	slowtraps


/* FPU not available in this context. */
GATE_ENTRY	0x07, entry_vec07_fpu_unavail, (ACC_PL_K | ACC_INTR_GATE)

/* do all of this with disabled interrupts */
	.p2align 4
	.type	entry_vec07_fpu_unavail,@function
entry_vec07_fpu_unavail:
	cld
	SAVE_SCRATCH
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	call	thread_handle_fputrap
in_handle_fputrap:
	CHECK_SANITY 16(%esp)			/* scratches ecx */
	test 	%eax, %eax
	jz 	.Lreal_fpu_exception
	RESTORE_SCRATCH
	iret 
.Lreal_fpu_exception:
	RESTORE_SCRATCH
	pushl	$(0)
	pushl	$(7)
	pusha							
	jmp	.L_slowtraps


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
#ifndef CONFIG_NO_FRAME_PTR
	pushl	%ebp
	movl	%esp,%ebp
#endif
	SAVE_SCRATCH
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	cld
.Ldo_timer_interrupt:
#ifdef CONFIG_NO_FRAME_PTR
	movl	12(%esp), %eax			/* ARG1: eip for logging */
#else
	movl	16(%esp), %eax			/* ARG1: eip for logging */
#endif
	call	thread_timer_interrupt		/* enter with disabled irqs */
in_timer_interrupt:
#ifndef CONFIG_NO_FRAME_PTR
	CHECK_SANITY 20(%esp)			/* scratches ecx */
#else
	CHECK_SANITY 16(%esp)			/* scratches ecx */
#endif
	RESTORE_SCRATCH
#ifndef CONFIG_NO_FRAME_PTR
	popl	%ebp
#endif
	iret

	.p2align 4
	.globl	entry_int_timer_slow
entry_int_timer_slow:
#ifndef CONFIG_NO_FRAME_PTR
	pushl	%ebp
	movl	%esp,%ebp
#endif
	SAVE_SCRATCH
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	cld

	call	thread_timer_interrupt_slow	/* enter with disabled irqs */
in_timer_interrupt_slow:
	jmp	.Ldo_timer_interrupt

	
	.p2align 4
	.globl	entry_int_timer_stop
entry_int_timer_stop:
#ifndef CONFIG_NO_FRAME_PTR
	pushl	%ebp
	movl	%esp,%ebp
#endif
	SAVE_SCRATCH
	cld
	call	thread_timer_interrupt_stop
#ifndef CONFIG_NO_FRAME_PTR
	CHECK_SANITY 20(%esp)			/* scratches ecx */
#else
	CHECK_SANITY 16(%esp)			/* scratches ecx */
#endif
	RESTORE_SCRATCH
#ifndef CONFIG_NO_FRAME_PTR
	popl	%ebp
#endif
	iret


/* other interrupts */

.macro	INTERRUPT int, name
	GATE_ENTRY	\int, entry_\name, (ACC_PL_K | ACC_INTR_GATE)
	.p2align 3
entry_\name:
	pushl	%eax
	movl	$ (\int - 0x20), %eax	/* ARG1: irqnum */
	jmp	.Lall_irqs
.endm

#ifndef CONFIG_SCHED_PIT
INTERRUPT	0x20, int0
#endif
/* spurious IRQ handlers */
GATE_ENTRY	0x27, entry_int_pic_ignore, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	0x2f, entry_int_pic_ignore, (ACC_PL_K | ACC_INTR_GATE)
#ifndef CONFIG_SCHED_RTC
INTERRUPT	0x28, int8
#endif

/**
 * Stub array for all IRQ vector entries in the IDT
 *
 * The immediate in the movabs shall be patched with the
 * address of an Irq_base object that is later attached to
 * the corresponding IDT vector.
 */
	.section ".entry.text.irqs", "ax", @progbits
	.global idt_irq_vector_stubs
	.align 64
idt_irq_vector_stubs:
.rept APIC_IRQ_BASE - 0x28
	push	%eax
	mov	$0, %eax
	jmp	__generic_irq_entry
.endr
        .previous


	.p2align 4
	.type	.Lall_irqs,@function
	.type	__generic_irq_entry,@function
	.global __generic_irq_entry
.Lall_irqs:
__generic_irq_entry:
	cld
	pushl	%edx
	pushl	%ecx
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	movl	12(%esp), %edx			/* ARG2: eip */
	call	irq_interrupt			/* enter with disabled irqs */
in_interrupt:
	CHECK_SANITY 16(%esp)			/* scratches ecx */
	popl	%ecx
	popl	%edx
	popl	%eax

entry_int_pic_ignore:
	iret

	.global	entry_int_pic_ignore
	.global	entry_int7
	.global	entry_intf

/****************************************************************************/
/* system calls                                                             */
/****************************************************************************/

GATE_ENTRY	0x30, entry_sys_ipc_c, (ACC_PL_U | ACC_INTR_GATE)

        .p2align 4
	.globl	entry_sys_ipc_c
entry_sys_ipc_c:
	cld
	pushl	%eax
	SAVE_STATE
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	ESP_TO_TCB_AT %ebx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%ebx)
	jnz	.Lalien_sys_ipc_c
	RESET_THREAD_CANCEL_AT %ebx
	call	sys_ipc_wrapper //ipc_short_cut_wrapper
in_sc_ipc1:
	CHECK_SANITY $3			/* scratches ecx */
	RESTORE_STATE_AFTER_IPC
	popl	%eax
	iret

	.globl	in_sc_ipc1

#ifdef CONFIG_JDB
	/* The slow variant of sys_ipc_entry is used when logging IPC */
        .p2align 4
	.globl	entry_sys_ipc_log
entry_sys_ipc_log:
	cld
	pushl	%eax
	SAVE_STATE
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	ESP_TO_TCB_AT %ebx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%ebx)
	jnz	.Lalien_sys_ipc_log
	RESET_THREAD_CANCEL_AT %ebx
	call	*syscall_table
in_slow_ipc4:
	CHECK_SANITY $3			/* scratches ecx */
	RESTORE_STATE_AFTER_IPC
	popl	%eax
	iret

	.globl	in_slow_ipc4
#endif // CONFIG_JDB

        // these labels help show_tcb to guess the thread state
	.globl	in_slowtrap
	.globl	in_page_fault
	.globl	in_handle_fputrap
	.globl	in_interrupt  
	.globl	in_timer_interrupt
	.globl	in_timer_interrupt_slow


#ifndef CONFIG_PF_UX
/* these functions are implemented in entry-native.S */
GATE_ENTRY	0x0a, entry_vec0a_invalid_tss, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	0x0f, entry_vec0f_apic_spurious_interrupt_bug, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE + 3), entry_apic_error_interrupt, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE + 4), entry_apic_spurious_interrupt, (ACC_PL_K | ACC_INTR_GATE)
#endif

#ifdef CONFIG_MP
GATE_ENTRY	(APIC_IRQ_BASE + 2), entry_ipi, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE - 2), entry_debug_ipi, (ACC_PL_K | ACC_INTR_GATE)
GATE_ENTRY	(APIC_IRQ_BASE - 1), entry_ipi_remote_request, (ACC_PL_K | ACC_INTR_GATE)
#endif

GATE_INITTAB_END

.Lalien_sys_ipc_c:
	PRE_ALIEN_IPC
	call	sys_ipc_wrapper
	POST_ALIEN_IPC

#if defined (CONFIG_JDB)
.Lalien_sys_ipc_log:
	PRE_ALIEN_IPC
	call	*syscall_table
	POST_ALIEN_IPC
#endif


#define OFS__VCPU_STATE__REGS_IRET (VAL__SIZEOF_TRAP_STATE - 20 + OFS__VCPU_STATE__TREX)

	.p2align 3
	.globl	leave_by_trigger_exception
leave_by_trigger_exception:
	subl	$12,%esp		/* clean up stack from previous
					 * CPL0-CPL0 iret */
	SAVE_SCRATCH
	call	thread_restore_exc_state
	RESTORE_SCRATCH
	pushl	$0x00
	pushl	$0xff
	pusha
	jmp	.L_slowtraps

	.p2align 3
	.globl	leave_by_vcpu_upcall
leave_by_vcpu_upcall:
	subl	$12,%esp		/* clean up stack from previous
					 * CPL0-CPL0 iret */
	SAVE_SCRATCH
	call	thread_restore_exc_state

	ESP_TO_TCB_AT %ecx
	mov OFS__THREAD__USER_VCPU(%ecx), %eax /* vcpu state pointer from TCB */
	mov OFS__THREAD__VCPU_STATE(%ecx), %ecx /* vcpu state pointer from TCB */
	addl $(OFS__VCPU_STATE__REGS_IRET), %ecx /* skip return frame */
	mov SCRATCH_REGISTER_SIZE(%esp),  %edx
	mov %edx, (%ecx)                                /* EIP */
	mov 8 + SCRATCH_REGISTER_SIZE(%esp),  %edx
	mov %edx, 8(%ecx)                               /* EFLAGS */
	mov 12 + SCRATCH_REGISTER_SIZE(%esp), %edx
	mov %edx, 12(%ecx)                              /* ESP */
	mov 16 + SCRATCH_REGISTER_SIZE(%esp), %edx
	mov %edx, 16(%ecx)                              /* SS */

	mov 0(%esp), %edx    /* EDX */
	mov %edx, -20(%ecx)
	mov 4(%esp), %edx    /* ECX */
	mov %edx, -16(%ecx)
	mov 8(%esp), %edx    /* EAX */
	mov %edx, -12(%ecx)

	lea 12(%esp), %edx
	lea -20(%ecx), %esp

	push %ebx            /* EBX */
	sub $4, %esp         /* SKIP PFA */
	push %ebp            /* EBP */
	push %esi            /* ESI */
	push %edi            /* EDI */

	/*add SCRATCH_REGISTER_SIZE, %esp*/

	pushl	REG_FS		/* we save the segment regs in the trap   */
	pushl	REG_GS		/* state, but we do not restore them.  We */
	pushl	%ds		/* rather reload them using               */
	pushl	%es		/* RESET_{KERNEL,USER}_SEGMENTS           */

	mov %edx, %esp
	mov -OFS__VCPU_STATE__REGS_IRET + OFS__VCPU_STATE__ENTRY_SP(%ecx), %edx
	mov %edx, 12(%esp)
	mov -OFS__VCPU_STATE__REGS_IRET + OFS__VCPU_STATE__ENTRY_IP(%ecx), %edx
	mov %edx, 0(%esp)
	movl $EFLAGS_IF, 8(%esp)
	iret
	
	.p2align 3
	.globl	vcpu_resume
vcpu_resume:
	mov %eax, %esp
	addl $(2*4), %esp // skip es and ds for now
	//popl %es
	//popl %ds
	popl REG_GS
	popl REG_FS
	popa
	addl $(2*4), %esp
	iret

	// fast return from Dirq::hit
	.align	16
	.globl	fast_ret_from_irq
fast_ret_from_irq:
	CHECK_SANITY $3				// scratches ecx
	RESTORE_STATE_AFTER_IPC
	popl	%eax
	andl	$0x7f, 4(%esp)			// if entered using sysenter
	iret

        .p2align 3
        .globl  leave_and_kill_myself
leave_and_kill_myself:
        // make space for a dummy Return_frame accessible by the callee
        subl    $(5*4),%esp                     // sizeof(Return_frame)
        call    thread_do_leave_and_kill_myself
        // does never return

