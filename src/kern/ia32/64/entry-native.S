
#include "config_tcbsize.h"
#include "config_gdt.h"
#include "globalconfig.h"
#include "idt_init.h"
#include <low_level.h>
#include "regdefs.h"
#include "shortcut.h"
#include "tcboffset.h"

	.p2align 4
	.globl	entry_vec01_debug
entry_vec01_debug:
/* XXX we have to check single step bug */
1:	push	$(0)
	push	$(1)
	push	%rax
	push	%rcx
	push	%rdx
	push	%rbx
	mov	%cr2,%rax
	push	%rax
	push	%rbp
	push	%rsi
	push	%rdi
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	jmp	slowtraps


	.p2align(4)
	.globl	entry_vec08_dbf
entry_vec08_dbf:
	push	$(0)
	push	$(8)
	push	%rax
	push	%rcx
	push	%rdx
	push	%rbx
	mov	%cr2,%rax
	push	%rax
	push	%rbp
	push	%rsi
	push	%rdi
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	mov	%rsp,%rdi		// 1st arg: trap state
	call	thread_handle_double_fault
	jmp	entry_vec08_dbf


	.p2align(4)
	.globl	entry_vec0a_invalid_tss
entry_vec0a_invalid_tss:
	andq	$0xffffffffffffbfff, 24(%rsp)
	add	$8, %rsp			/* skip error code */
	iretq

/* PPro spurious interrupt bug: 
 * See "Pentium Pro Processor Specification Update / January 1999"
 * Erratum "Virtual Wire mode through local APIC may cause int 15"
 * This exception can be silently ignored */
	.p2align(4)
	.globl	entry_vec0f_apic_spurious_interrupt_bug
entry_vec0f_apic_spurious_interrupt_bug:
	incl	apic_spurious_interrupt_bug_cnt
	iretq


/* XXX has to be fixed */
/* APIC error interrupt */
	.p2align(4)
	.globl	entry_apic_error_interrupt
entry_apic_error_interrupt:
	cld
	SAVE_SCRATCH
	lea	SCRATCH_REGISTER_SIZE(%rsp), %rdi
	call	apic_error_interrupt
	RESTORE_SCRATCH
	iretq

/* Intel Architecture Software Developer's Manual Volume 3,
 * Advanced Programmable Interrupt Controller (APIC):
 * Spurious Interrupt: "If at the time the INTA cycle is issued, the
 * interupt that was to be dispensed has become masked (programmed by
 * software), the local APIC will deliver a spurious-interrupt vector." */
	.p2align(4)
	.globl	entry_apic_spurious_interrupt
entry_apic_spurious_interrupt:
	incl	apic_spurious_interrupt_cnt
	iretq

	.p2align(4)
	.global	entry_int_apic_ignore
entry_int_apic_ignore:
	push	%rcx
	push	%rdx
	mov	apic_io_base, %rcx
	mov	0xf0(%rcx), %edx
	movl	$0, 0xb0(%rcx)
	pop	%rdx
	pop	%rcx
	iretq

#if defined(CONFIG_JDB)

	.p2align(4)
	.global entry_syscall_log
	.global entry_sys_fast_ipc_log
entry_syscall_log:
entry_sys_fast_ipc_log:
	push	%r11				/* save user rflags */
	push	$(GDT_CODE_USER | SEL_PL_U | 0x80)	/* fake user cs */
	push	%rcx				/* save user rip */
	sub     $16, %rsp
	SAVE_STATE_SYSEXIT
	ESP_TO_TCB_AT %rbx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%rbx)
	jnz	alien_sys_fast_ipc_log
	RESET_THREAD_CANCEL_AT %rbx

	call	*syscall_table
in_slow_ipc5:
	RESTORE_STATE_SYSEXIT
	mov     16(%rsp), %rcx /* load user rip */
        /* make RIP canonical, workaround for intel IA32e flaw */
        shl     $16, %rcx
        sar     $16, %rcx
	mov	32(%rsp), %r11				/* load user rflags */
	mov	40(%rsp), %rsp				/* load user rsp */
	sysretq

alien_sys_fast_ipc_log:
	ALIEN_SYSCALL syscall="call *syscall_table"


	.globl	in_slow_ipc5

#endif // CONFIG_JDB

	.p2align(4)
	.global entry_syscall_c
	.global entry_sys_fast_ipc_c
entry_syscall_c:
entry_sys_fast_ipc_c:
	push	%r11				/* save user rflags */
	push	$(GDT_CODE_USER | SEL_PL_U | 0x80)	/* fake user cs */
	push	%rcx				/* save user rip */
	sub     $16, %rsp
	SAVE_STATE_SYSEXIT
	ESP_TO_TCB_AT %rbx
	testl	$VAL__Thread_alien_or_vcpu_user, OFS__THREAD__STATE (%rbx)
	jnz	alien_sys_fast_ipc_c
	RESET_THREAD_CANCEL_AT %rbx
	
	call    sys_ipc_wrapper
in_sc_ipc2:
	.globl	fast_ret_from_irq
fast_ret_from_irq:
	RESTORE_STATE_SYSEXIT
	
	mov     16(%rsp), %rcx /* load user rip */
        /* make RIP canonical, workaround for intel IA32e flaw */
        shl     $16, %rcx
        sar     $16, %rcx
	mov	32(%rsp), %r11				/* load user rflags */
	mov	40(%rsp), %rsp				/* load user rsp */
	sysretq

alien_sys_fast_ipc_c:
	ALIEN_SYSCALL syscall="call sys_ipc_wrapper"


	.globl	in_sc_ipc2

	.p2align(4)
	.globl	leave_from_syscall_by_iret
leave_from_syscall_by_iret:
	RESTORE_STATE_SYSEXIT
	add $16, %rsp
	iretq

	.bss
	.space	4096
	.global	dbf_stack_top
dbf_stack_top:

