#ifndef LOW_LEVEL_H
#define LOW_LEVEL_H

#include "asm.h"
#include "regdefs.h"
#include "shortcut.h"
#include "tcboffset.h"

#define REGISTER_SIZE 4

	.macro save_all_regs
	pusha
	.endm

	.macro restore_all_regs
	popa
	.endm

/* be sure that we do not enable the interrupts here! */
	.macro	RESTORE_IOPL
	pushl	16(%esp)
	andl	$~EFLAGS_IF,(%esp)
	popf
	.endm

/** Some shared macros and stuff shared by the two different 
 *  assembler kernel entry sources.
 */

/** Setting up ds/es resp. fs/gs when entering/
 * leaving the kernel is not neccessary anymore
 * since the user can only load the null selector
 * without exception. But then, the first access
 * to code/data with the wrong selector loaded
 * raises an exception 13 which is handled properly.
 */
	.macro	RESET_KERNEL_SEGMENTS_FORCE_DS_ES
	mov	%ds, %cx
	cmp	$ (GDT_DATA_USER|SEL_PL_U), %cx
	jne	9f

	mov	%es, %cx
	cmpw	$ (GDT_DATA_USER|SEL_PL_U), %cx
	je	8f

9:	movw    $ (GDT_DATA_USER|SEL_PL_U), %cx
	movl    %ecx,%ds
	movl    %ecx,%es
8:
	.endm

	.macro	DO_SYSEXIT
	addl	$8, %esp	/* skip ecx & edx */
	popl	%esi
	popl	%edi
	popl	%ebx
	//CHECK_SANITY $3		/* scratches ecx */
	RESTORE_IOPL
	movl	4(%esp), %eax
	movl	8(%esp), %edx	/* user eip */
	movl	20(%esp), %ecx	/* user esp */
	//subl	$2, %edx	/* adj. eip */
	sti			/* the interrupts are enabled _after_ the
				 * next instruction (see Intel Ref-Manual) */
	sysexit
	.endm

	.macro	RESET_THREAD_CANCEL_AT reg
	andl	$~(VAL__Thread_cancel), OFS__THREAD__STATE (\reg)
	.endm

	.macro	RESET_THREAD_IPC_MASK_AT reg
	andl	$~VAL__Thread_ipc_mask, OFS__THREAD__STATE (\reg)
	.endm

	.macro	ESP_TO_TCB_AT reg
	movl	%esp, \reg
	andl	$~(THREAD_BLOCK_SIZE - 1), \reg
	.endm

	.macro	SAVE_STATE
	pushl	%ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx
	.endm

	.macro	RESTORE_STATE
	popl	%ecx
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp
	.endm

	.macro	RESTORE_STATE_AFTER_IPC
	addl	$4, %esp
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebx
	addl	$4, %esp
	.endm

#define SCRATCH_REGISTER_SIZE 12
	.macro	SAVE_SCRATCH
	push	%eax
	push	%ecx
	push	%edx
	.endm

	.macro	RESTORE_SCRATCH
	pop	%edx
	pop	%ecx
	pop	%eax
	.endm

	.macro	IRET_INSN
	iret
	.endm

#define PAGE_FAULT_ADDR	%cr2
#define PAGE_DIR_ADDR	%cr3

#endif //LOW_LEVEL_H
