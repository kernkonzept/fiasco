#ifndef ASM_SANITY_CHECK_H
#define ASM_SANITY_CHECK_H

#include "globalconfig.h"
#include "shortcut.h"
#include "tcboffset.h"

#define kdb_ke_asm(msg)		\
	int3			;\
	jmp	9f		;\
	.ascii	msg		;\
9:

#define MAY_FAULT(insn, label) \
	.global label;         \
	label: insn

//.macro REGS this_ptr, dst
//	leal	(THREAD_BLOCK_SIZE - OFS__ENTRY_FRAME__MAX)(\this_ptr), \dst
//.endm

#ifdef CONFIG_BEFORE_IRET_SANITY

	// some sanity checks before return to user
	// touches ecx and the thread-stack
	.macro CHECK_SANITY csseg

	.ifnc	\csseg,$3
	// don't check if we return to kernelmode
	movl    \csseg, %ecx
	andb    $3, %cl
	jz      2f
	.endif

	pushl	%ebx
	pushl	%edx
	ESP_TO_TCB_AT %ebx

	// are we holding locks?
	cmpl	$0, OFS__THREAD__LOCK_CNT(%ebx)
	jne	1f
	.text	1
1:	kdb_ke_asm("Before IRET: Thread holds a lock")
	jmp	1f
	.previous
1:

	// check for the right thread state
	// (cancel and fpu_owner might also be set)
	movl	OFS__THREAD__STATE(%ebx), %edx
	andl	$~(VAL__Thread_cancel | VAL__Thread_fpu_owner | VAL__Thread_alien_or_vcpu_user | VAL__Thread_dis_alien | VAL__Thread_vcpu_state_mask), %edx
	cmpl	$(VAL__Thread_ready), %edx
	jne	1f
	.text	1
1:	kdb_ke_asm("Before IRET: Wrong thread state")
	jmp	1f
	.previous
1:

	popl	%edx
	popl	%ebx
2:

	.endm

#else	// ! DO_SANITY_CHECKS_BEFORE_IRET

	.macro CHECK_SANITY csseg
	.endm

#endif // DO_SANITY_CHECKS_BEFORE_IRET

#endif // ASM_SANITY_CHECK_H
