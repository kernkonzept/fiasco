
#include "config_tcbsize.h"
#include "config_gdt.h"
#include "globalconfig.h"
#include "idt_init.h"
#include <low_level.h>
#include "regdefs.h"
#include "tcboffset.h"

	.section ".entry.text.debug_exc", "ax", @progbits
	.p2align 4
	.globl	entry_vec01_debug
entry_vec01_debug:
/* XXX we have to check single step bug */
1:	SWITCH_TO_KERNEL_CR3 0
	push	$(0)
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


	.section ".entry.text.dbf", "ax", @progbits
	.p2align 4
	.globl	entry_vec08_dbf
entry_vec08_dbf:
	SWITCH_TO_KERNEL_CR3 0
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
	IA32_IBRS_CLOBBER
	mov	%rsp,%rdi		// 1st arg: trap state
	call	thread_handle_double_fault
	jmp	entry_vec08_dbf


	.section ".entry.text.tss", "ax", @progbits
	.p2align 4
	.globl	entry_vec0a_invalid_tss
entry_vec0a_invalid_tss:
	andq	$0xffffffffffffbfff, 24(%rsp)
	add	$8, %rsp			/* skip error code */
	iretq

	.section ".entry.text.apic", "ax", @progbits
/* PPro spurious interrupt bug: 
 * See "Pentium Pro Processor Specification Update / January 1999"
 * Erratum "Virtual Wire mode through local APIC may cause int 15"
 * This exception can be silently ignored */
	.p2align 4
	.globl	entry_vec0f_apic_spurious_interrupt_bug
entry_vec0f_apic_spurious_interrupt_bug:
#ifndef CONFIG_KERNEL_ISOLATION
	incl	apic_spurious_interrupt_bug_cnt
#endif
	iretq


/* XXX has to be fixed */
/* APIC error interrupt */
	.p2align 4
	.globl	entry_apic_error_interrupt
entry_apic_error_interrupt:
	cld
	SWITCH_TO_KERNEL_CR3 0
	SAVE_SCRATCH
	lea	SCRATCH_REGISTER_SIZE(%rsp), %rdi
	call	apic_error_interrupt
	RESTORE_SCRATCH
	SAFE_IRET

/* Intel Architecture Software Developer's Manual Volume 3,
 * Advanced Programmable Interrupt Controller (APIC):
 * Spurious Interrupt: "If at the time the INTA cycle is issued, the
 * interupt that was to be dispensed has become masked (programmed by
 * software), the local APIC will deliver a spurious-interrupt vector." */
	.p2align 4
	.globl	entry_apic_spurious_interrupt
entry_apic_spurious_interrupt:
#ifndef CONFIG_KERNEL_ISOLATION
	incl	apic_spurious_interrupt_cnt
#endif
	iretq

	.p2align 4
	.global	entry_int_apic_ignore
entry_int_apic_ignore:
	SWITCH_TO_KERNEL_CR3 0
	push	%rcx
	push	%rdx
	cmpb	$0, apic_use_x2		// code patching not worth the effort
	jne	1f
	// APIC
	mov	apic_io_base, %rcx
	mov	0xf0(%rcx), %edx	// SPIV
	movl	$0, 0xb0(%rcx)		// EOI
	jmp	2f
	// x2APIC
1:	push	%rax
	movl	$0x80f, %ecx		// SPIV
	rdmsr
	movl	$0x80b, %ecx		// EOI
	xorl	%eax, %eax
	xorl	%edx, %edx
	wrmsr
	pop	%rax
2:	pop	%rdx
	pop	%rcx
	SAFE_IRET

#if defined(CONFIG_JDB)
	.section ".entry.text.syscalls_log", "ax", @progbits
	.p2align 4
	.global entry_syscall_log
	.global entry_sys_fast_ipc_log
entry_syscall_log:
entry_sys_fast_ipc_log:
#ifndef CONFIG_KERNEL_ISOLATION
	/* with kernel isolation wee need r11 as scratch in the entry
	   code and save it there */
	push	%r11				/* save user rflags */
#endif
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
	SAFE_SYSRET

alien_sys_fast_ipc_log:
	ALIEN_SYSCALL syscall="call *syscall_table"


	.globl	in_slow_ipc5

#endif // CONFIG_JDB

	.section ".entry.text.syscalls", "ax", @progbits
	.p2align 4
	.global entry_syscall_c
	.global entry_sys_fast_ipc_c
entry_syscall_c:
entry_sys_fast_ipc_c:
#ifndef CONFIG_KERNEL_ISOLATION
	/* with kernel isolation wee need r11 as scratch in the entry
	   code and save it there */
	push	%r11				/* save user rflags */
#endif
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
	SAFE_SYSRET

alien_sys_fast_ipc_c:
	ALIEN_SYSCALL syscall="call sys_ipc_wrapper"


	.globl	in_sc_ipc2

	.p2align 4
	.globl	leave_from_syscall_by_iret
leave_from_syscall_by_iret:
	RESTORE_STATE_SYSEXIT
	add $16, %rsp
	SAFE_IRET

	.bss
	.space	4096
	.global	dbf_stack_top
dbf_stack_top:

	.section ".text.syscall_entry"

#ifdef CONFIG_KERNEL_NX

/*
 * This code shall be mapped to Kentry_cpu_page_text, exactly one page above
 * Kentry_cpu_page, which makes it possible to refer to the data without
 * clobbering a register.
 */
#define CPUE_OFFSET syscall_entry_code - 0x1000

	.align 0x1000

#else

/* This code shall be copied to Kentry_cpu_syscall_entry */
#define CPUE_OFFSET syscall_entry_code - 0x30

#endif
	.global syscall_entry_code
syscall_entry_code:
	mov	%rsp, (CPUE_OFFSET + CPUE_SCRATCH_OFS)(%rip)
	mov	(CPUE_OFFSET + CPUE_CR3_OFS)(%rip), %rsp
	mov	%rsp, %cr3
	mov	(CPUE_OFFSET + CPUE_KSP_OFS)(%rip), %rsp
	pushq	$(GDT_DATA_USER | 3)
	pushq	(CPUE_OFFSET + CPUE_SCRATCH_OFS)(%rip)
	push	%r11				/* save user rflags */
	IA32_IBRS
	.global syscall_entry_reloc
syscall_entry_reloc:
	mov	$entry_sys_fast_ipc_c, %r11
	jmp	*%r11
	.global syscall_entry_code_end
syscall_entry_code_end:

#ifdef CONFIG_KERNEL_ISOLATION
	.section ".entry.text.safe_sysret"
	.global safe_sysret
safe_sysret:
	// The 'xor' instructions clearing the lower 32 bits clear the entire
	// register but are 1 byte shorter (at least when using the 'eXX'
	// registers).
	mov	%rax, %cr3
	xor	%eax, %eax
	xor	%ebx, %ebx
	/* make RIP canonical, workaround for Intel IA32e flaw */
	shl     $16, %rcx
	sar     $16, %rcx
	xor	%edx, %edx
	xor	%ebp, %ebp
	xor	%esi, %esi
	xor	%r8d, %r8d
	xor	%r9d, %r9d
	xor	%r10d, %r10d
	xor	%r12d, %r12d
	xor	%r13d, %r13d
	xor	%r14d, %r14d
	xor	%r15d, %r15d
	sysretq

	.section ".entry.text.safe_iret"
	.global safe_iret
safe_iret:
	cli
	sub	$24, %rsp
	push	%r14
	mov	40(%rsp), %r14
	test	$3, %r14
	jz	1f

	mov	$VAL__MEM_LAYOUT__KENTRY_CPU_PAGE, %r14
	mov	%r13, CPUE_STACK(-56, %r14)
# if defined(CONFIG_INTEL_IA32_BRANCH_BARRIERS) || defined(CONFIG_INTEL_MDS_MITIGATION)
	/* check for exit flags (need IBTPB?) */
	mov	CPUE_EXIT(%r14), %r13
        // Here, CPUE_EXIT_NEED_IBPB and CPUE_EXIT_NEED_VERW are always set
        // together, therefore only a single test is required.
	test	$(CPUE_EXIT_NEED_IBPB), %r13
	jz	22f
	andl	$(~(CPUE_EXIT_NEED_IBPB|CPUE_EXIT_NEED_VERW)), CPUE_EXIT(%r14)
#  ifdef CONFIG_INTEL_IA32_BRANCH_BARRIERS
	IA32_IBPB
#  endif
#  ifdef CONFIG_INTEL_MDS_MITIGATION
        /* Only the memory-operand variant guarantees the CPU buffer flush
         * functionality according to the documentation. */
	verw	verw_gdt_data_kernel
#  endif
22:
# endif
	mov	(%rsp), %r13
	mov	%r13, CPUE_STACK(-48, %r14)
	mov	32(%rsp), %r13
	mov	%r13, CPUE_STACK(-40, %r14)
	mov	40(%rsp), %r13
	mov	%r13, CPUE_STACK(-32, %r14)
	mov	48(%rsp), %r13
	mov	%r13, CPUE_STACK(-24, %r14)
	mov	56(%rsp), %r13
	mov	%r13, CPUE_STACK(-16, %r14)
	mov	64(%rsp), %r13
	mov	%r13, CPUE_STACK( -8, %r14)
	lea	CPUE_STACK(-56, %r14), %rsp
	mov	CPUE_CR3U(%r14), %r13
	mov	%r13, %cr3
	pop	%r13
	pop	%r14
6:	iretq

1:	pop	%r14
	add	$24, %rsp
5:	iretq	/* direct iret to kernel mode */

.global safe_iret_insn
	ASM_KEX 6b, 0, iretq_exception_handler
	/* the in kernel iret should actually never trigger this */
	ASM_KEX 5b, 0, iretq_exception_handler

# ifdef CONFIG_INTEL_MDS_MITIGATION
        .p2align 1
.global verw_gdt_data_kernel
verw_gdt_data_kernel:
        .word   GDT_DATA_KERNEL
# endif
#endif

