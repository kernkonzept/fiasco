#ifndef LOW_LEVEL_H
#define LOW_LEVEL_H

#include "asm.h"
#include "regdefs.h"
#include "shortcut.h"
#include "tcboffset.h"


#define REGISTER_SIZE 8

#define CPUE_STACK(x, reg) (x + 0x20)(reg)
#define CPUE_CR3_OFS 0
#define CPUE_KSP_OFS 8
#define CPUE_KSP(reg) 8(reg)
#define CPUE_CR3(reg) 0(reg)
#define CPUE_EXIT(reg) 16(reg)
#define CPUE_EXIT_NEED_IBPB 1

#if defined(CONFIG_KERNEL_ISOLATION) && defined(CONFIG_INTEL_IA32_BRANCH_BARRIERS)
.macro IA32_IBRS_CLOBBER
	mov $0x48, %ecx
	mov $0, %edx
	mov $3, %eax
	wrmsr
.endm
.macro IA32_IBRS
	pushq %rax
	pushq %rcx
	pushq %rdx
	IA32_IBRS_CLOBBER
	popq %rdx
	popq %rcx
	popq %rax
.endm
.macro IA32_IBPB
	pushq %rax
	pushq %rcx
	pushq %rdx
	mov	$0x49, %ecx
	xor	%edx, %edx
	mov	$1, %eax
	wrmsr
	popq %rdx
	popq %rcx
	popq %rax
.endm
#else
.macro IA32_IBRS_CLOBBER
.endm
.macro IA32_IBRS
.endm
#endif

.macro SAFE_SYSRET
	/* make RIP canonical, workaround for intel IA32e flaw */
	shl     $16, %rcx
	sar     $16, %rcx
#ifdef CONFIG_KERNEL_ISOLATION
	mov	$0xffff817fffffc000, %r15
#ifdef CONFIG_INTEL_IA32_BRANCH_BARRIERS
	mov	CPUE_EXIT(%r15), %r11
	test	$(CPUE_EXIT_NEED_IBPB), %r11
	jz	333f
	and	$(~CPUE_EXIT_NEED_IBPB), %r11
	mov	%r11, CPUE_EXIT(%r15)
	IA32_IBPB
333:
#endif
	mov	CPUE_CR3(%r15), %r15
	or	$0x1000, %r15
#endif
	mov	32(%rsp), %r11				/* load user rflags */
	mov	40(%rsp), %rsp				/* load user rsp */
#ifdef CONFIG_KERNEL_ISOLATION
	mov	%r15, %cr3
#endif
	sysretq
.endm

.macro SAFE_IRET
#ifndef CONFIG_KERNEL_ISOLATION
	iretq
#else
	jmp	safe_iret
#endif
.endm

.macro  SWITCH_TO_KERNEL_CR3 err
#ifdef CONFIG_KERNEL_ISOLATION
	push	%r14
	.if \err == 1
	  mov	24(%rsp), %r14
	.else
	  mov	16(%rsp), %r14
	.endif
	test	$3, %r14
	jz	5551f

	push	%r13
	mov	$0xffff817fffffc000, %r13
	mov	CPUE_CR3(%r13), %r14
	mov	%r14, %cr3
	mov	CPUE_KSP(%r13), %r14
	.if \err == 1
	  sub	$56, %r14
	  mov	56(%rsp), %r13
	  mov	%r13, 48(%r14)
	.else
	  sub	$48, %r14
	.endif
	mov	48(%rsp), %r13
	mov	%r13, 40(%r14)
	mov	40(%rsp), %r13
	mov	%r13, 32(%r14)
	mov	32(%rsp), %r13
	mov	%r13, 24(%r14)
	mov	24(%rsp), %r13
	mov	%r13, 16(%r14)
	mov	16(%rsp), %r13
	mov	%r13, 8(%r14)
	mov	8(%rsp), %r13
	mov	%r13, (%r14)
	mov	(%rsp), %r13
	mov	%r14, %rsp

5551:
	pop	%r14
#endif
.endm

.macro  PRE_ALIEN_IPC target=slowtraps
	btrl	$17, OFS__THREAD__STATE (%rbx)	/* Thread_dis_alien */
	jc	1f
	movq	$253, (16*8)(%rsp)
	movq	$0,   (17*8)(%rsp)
	sub	$2, (16*8 + 2*8)(%rsp) /* reset RIP to syscall */
	jmp	\target
1:	/* do alien IPC and raise a trap afterwards */
	RESET_THREAD_CANCEL_AT %rbx
.endm

.macro  POST_ALIEN_IPC target=slowtraps
	movq	$253, (16*8)(%rsp)
	movq	$1,   (17*8)(%rsp)
	jmp	\target
.endm

.macro ALIEN_SYSCALL syscall, trap_target=slowtraps
	PRE_ALIEN_IPC \trap_target
	\syscall
	POST_ALIEN_IPC \trap_target
.endm


.macro	SAVE_STATE save_cr2=0 timeout_reg=%rcx
	push	\timeout_reg
	push	%rdx
	push	%rbx
.if \save_cr2 != 0
	mov	%cr2, %rbx
	push	%rbx
.else
	sub	$8, %rsp // reserved
.endif
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
.endm

.macro	RESTORE_STATE
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rbp
	add	$8, %rsp
	pop	%rbx
	pop	%rdx
	pop	%rcx
.endm

.macro save_all_regs
	push	%rax
	SAVE_STATE 1
.endm

.macro restore_all_regs
	RESTORE_STATE
	pop	%rax
.endm

.macro	SAVE_STATE_SYSEXIT
	push	%rax
	SAVE_STATE timeout_reg=%r8
.endm

.macro	RESTORE_STATE_SYSEXIT
	RESTORE_STATE
	pop	%rax
.endm

	.macro	RESET_THREAD_CANCEL_AT reg
	andl	$~(VAL__Thread_cancel | VAL__Thread_dis_alien), OFS__THREAD__STATE (\reg)
	.endm

	.macro	ESP_TO_TCB_AT reg
	mov	%rsp, \reg
	andq	$~(THREAD_BLOCK_SIZE - 1), \reg
	.endm


#define SCRATCH_REGISTER_SIZE 72
	.macro	SAVE_SCRATCH
	push	%rdi
	push	%rsi
	push	%rax /* must be ax cx dx for pagein_tcb_request */
	push	%rcx
	push	%rdx
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	.endm

	.macro	RESTORE_SCRATCH
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rdx
	pop	%rcx
	pop	%rax
	pop	%rsi
	pop	%rdi
	.endm

	.macro	IRET_INSN
	iretq
	.endm

#define PAGE_FAULT_ADDR	%cr2
#define PAGE_DIR_ADDR	%cr3

#endif //LOW_LEVEL_H
