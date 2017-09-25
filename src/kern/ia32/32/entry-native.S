
#include "config_tcbsize.h"
#include "config_gdt.h"
#include "globalconfig.h"
#include "idt_init.h"
#include <low_level.h>
#include "regdefs.h"
#include "shortcut.h"
#include "tcboffset.h"

.macro  PRE_FAST_ALIEN_IPC
	btrl	$17, OFS__THREAD__STATE (%ebx)	/* Thread_dis_alien */
	jc	1f
	RESTORE_STATE
	popl	%eax
	andl	$0x7f,4(%esp)
	pushl	$(0x30 << 3 | 2)
	pushl	$(0xd)
	pusha
	/* emulate entry code in KIP */
	mov	40(%esp), %ebp         /* offset 40: eip */
	mov	%ebp, 16(%esp)         /* offset 16: ebx */
	movl	$(VAL__MEM_LAYOUT__SYSCALLS + 4f - 3f), 40(%esp)
	mov	52(%esp), %ebp         /* offset 52: sp */
	mov	%ebp, 8(%esp)          /* offset  8: ebp */
	jmp	slowtraps
1:	/* do alien IPC and raise a trap afterwards */
	RESET_THREAD_CANCEL_AT %ebx

	.pushsection ".comment" /* discarded section */
3: /* must be the same code as code before sysenter in syscall-page.... */
	pop	%ebx
	mov	%esp, %ebp
4: /* the code between 3 and 4 shall never be executed, it is just to calculate
    * the offset for the sysenter insn in the KIP / syscall page
    */
	.popsection

.endm

.macro  POST_FAST_ALIEN_IPC
	RESTORE_STATE_AFTER_IPC
	popl	%eax
	andl	$0x7f,4(%esp)
	pushl	$(0x30 << 3 | 6)
	pushl	$(0xd)
	pusha
	jmp	slowtraps
.endm

	.p2align 4
	.globl	entry_vec01_debug
entry_vec01_debug:
	/* Check whether user entered the kernel with TF enabled,
	 * check priv-level to verify that a priv-change happened */
	testb   $3, %ss:4(%esp)
	jnz	1f
	cmpl	$entry_sys_fast_ipc_c, %ss:(%esp)
	je	2f
#if defined(CONFIG_JDB)
	cmpl	$entry_sys_fast_ipc_log, %ss:(%esp)
	je	2f
#endif
1:	pushl	$0
	pushl	$1
	pusha
	jmp	slowtraps


2:	mov	12(%esp), %esp
	pushl	$(GDT_DATA_USER|SEL_PL_U)
	push	%ebp
	pushl	$(EFLAGS_IF | EFLAGS_TF)
	pushl	$(GDT_CODE_USER|SEL_PL_U)
	push	%ebx
	cld

	pushl	%eax
	pushl	$0 // %ebp
	pushl	$0 // %ebx
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	ESP_TO_TCB_AT %ebx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%ebx)
	jnz	.Ldbg_alien_sys_fast_ipc_log
	RESET_THREAD_CANCEL_AT %ebx
	call	*syscall_table
	CHECK_SANITY $3			/* scratches ecx */
	RESTORE_STATE_AFTER_IPC
	popl	%eax
	iret

.Ldbg_alien_sys_fast_ipc_log:
	PRE_FAST_ALIEN_IPC
	push    $ret_from_fast_alien_ipc
	jmp     *syscall_table


	.globl	entry_vec0a_invalid_tss
entry_vec0a_invalid_tss:
	andl	$0xffffbfff, %ss:12(%esp)
	addl	$4, %esp			/* skip error code */
	iret


	.globl	entry_vec08_dbf
entry_vec08_dbf:
#if 0 /* XXX: disable debug feature reset on double fault */
	testl	$0xffffffff,CPU_DEBUGCTL_BUSY
	jnz	thread_handle_double_fault
	SAVE_SCRATCH
	movl	CPU_DEBUGCTL_RESET, %eax
	xorl	%edx, %edx
	movl	$0x1d9,%ecx
	wrmsr
	RESTORE_SCRATCH
#endif
	jmp	thread_handle_double_fault


/* PPro spurious interrupt bug: 
 * See "Pentium Pro Processor Specification Update / January 1999"
 * Erratum "Virtual Wire mode through local APIC may cause int 15"
 * This exception can be silently ignored */
	.p2align(4)
	.globl	entry_vec0f_apic_spurious_interrupt_bug
entry_vec0f_apic_spurious_interrupt_bug:
	pushl	%ecx
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	incl	apic_spurious_interrupt_bug_cnt
	popl	%ecx
	iret

/* APIC error interrupt */
	.p2align(4)
	.globl	entry_apic_error_interrupt
entry_apic_error_interrupt:
	cld
	SAVE_SCRATCH
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	leal	SCRATCH_REGISTER_SIZE(%esp), %eax /* &Return_frame */
	call	apic_error_interrupt
	RESTORE_SCRATCH
	iret

/* Intel Architecture Software Developer's Manual Volume 3,
 * Advanced Programmable Interrupt Controller (APIC):
 * Spurious Interrupt: "If at the time the INTA cycle is issued, the
 * interupt that was to be dispensed has become masked (programmed by
 * software), the local APIC will deliver a spurious-interrupt vector." */
	.p2align(4)
	.globl	entry_apic_spurious_interrupt
entry_apic_spurious_interrupt:
	pushl	%ecx
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	incl	apic_spurious_interrupt_cnt
	popl	%ecx
	iret

	.p2align(4)
	.global	entry_int_apic_ignore
entry_int_apic_ignore:
	pushl	%ecx
	pushl	%edx
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	mov	apic_io_base, %ecx
	mov	0xf0(%ecx), %edx
	movl	$0, 0xb0(%ecx)
	popl	%edx
	popl	%ecx
	iret

#if defined(CONFIG_JDB)

	.p2align(4)
	.global entry_sys_fast_ipc_log
entry_sys_fast_ipc_log:
	pop	%esp
	pushl	$(GDT_DATA_USER|SEL_PL_U) /* user ss */
	pushl   %ebp 			  // user esp
	pushl	$EFLAGS_IF                // fake user eflags
	cld
	// Fake user cs. This cs value is never used with exception
	// that the thread is ex_regs'd before we leave with sysexit.
	// lthread_ex_regs has to check user cs for that value. If
	// it is faked, the thread would leave the kernel by sysexit
	// and the thread is in the slow ipc path. Sysexit would
	// adapt the user eip (by subtracting 2) to ensure the user
	// executes the "mov %ebp,%edx" sequence. This is wrong if
	// the thread is ex_regs'd. In that case, we modify the return
	// value from "call dispatch_syscall" to an alternate exit
	// path using "iret".
	pushl	$(GDT_CODE_USER|SEL_PL_U|0x80)	/* user cs */
	pushl	%ebx				/* user eip */

	pushl	%eax
	pushl	$0 // %ebp
	pushl	$0 // %ebx
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx

	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	ESP_TO_TCB_AT %ebx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%ebx)
	jnz	.Lalien_sys_fast_ipc_log
	RESET_THREAD_CANCEL_AT %ebx
	call	*syscall_table
in_slow_ipc5:
	DO_SYSEXIT

.Lalien_sys_fast_ipc_log:
	PRE_FAST_ALIEN_IPC
	push    $ret_from_fast_alien_ipc
	jmp     *syscall_table

	.globl	in_slow_ipc5

#endif // CONFIG_JDB


	.p2align(4)
	.global entry_sys_fast_ipc_c
entry_sys_fast_ipc_c:
	pop	%esp
	pushl	$(GDT_DATA_USER|SEL_PL_U)	/* user ss */
	pushl	%ebp				// push user SP (get in ebp)
	pushl	$EFLAGS_IF                // fake user eflags
	cld
	// Fake user cs. This cs value is never used with exception
	// that the thread is ex_regs'd before we leave with sysexit.
	// lthread_ex_regs has to check user cs for that value. If
	// it is faked, the thread would leave the kernel by sysexit
	// and the thread is in the slow ipc path. Sysexit would
	// adapt the user eip (by subtracting 2) to ensure the user
	// executes the "mov %ebp,%edx" sequence. This is wrong if
	// the thread is ex_regs'd. In that case, we modify the return
	// value from "call dispatch_syscall" to an alternate exit
	// path using "iret".
	pushl	$(GDT_CODE_USER|SEL_PL_U|0x80)	/* user cs */
	pushl	%ebx				// push user return address

	pushl	%eax
	pushl	$0 				// ebp
	pushl	$0 // %ebx
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx				/* save ecx */
	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	ESP_TO_TCB_AT %ebx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%ebx)
	jnz	.Lalien_sys_fast_ipc_c
	RESET_THREAD_CANCEL_AT %ebx
	call    sys_ipc_wrapper
in_sc_ipc2:
	DO_SYSEXIT

	.global ret_from_fast_alien_ipc /* used in ex-regs */
/* ex-regs patches the return address of the call to the 'sys_ipc' function
 * in the case of sysenter, because we need to leave the kernel by iret
 * to prevent the need of user-level trampoline code.
 * In the case of alien ipc there must be a special case because otherwise the
 * second alien exception is not generated. Therefore ex-regs uses 
 * 'ret_from_fast_alien_ipc' to determine this case and patches the return
 * address with 'leave_alien_from_sysenter_by_iret' instead of
 * 'leave_from_sysenter_by_iret'. (see Thread::user_ip in thread-ia32-ux.cpp)
 */


.Lalien_sys_fast_ipc_c:
	cld
	PRE_FAST_ALIEN_IPC
	call    sys_ipc_wrapper
ret_from_fast_alien_ipc:
	POST_FAST_ALIEN_IPC

	.globl	in_sc_ipc2

.macro	LEAVE_SE_BY_IRET
	ESP_TO_TCB_AT %ebx
	RESET_THREAD_IPC_MASK_AT %ebx
	RESTORE_STATE_AFTER_IPC
	popl    %eax
.endm

	.p2align(4)
	.globl	leave_from_sysenter_by_iret
leave_from_sysenter_by_iret:
	LEAVE_SE_BY_IRET
	iret

	.globl	leave_alien_from_sysenter_by_iret
leave_alien_from_sysenter_by_iret:
	LEAVE_SE_BY_IRET
	pushl	$(0x30 << 3 | 6)
	pushl	$(0xd)
	pusha
	jmp	slowtraps

	.bss
	.space	4096
	.global	dbf_stack_top
dbf_stack_top:
