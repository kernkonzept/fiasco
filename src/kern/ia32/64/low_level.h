#ifndef LOW_LEVEL_H
#define LOW_LEVEL_H

#include "asm.h"
#include "regdefs.h"
#include "shortcut.h"
#include "tcboffset.h"


#define REGISTER_SIZE 8

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
