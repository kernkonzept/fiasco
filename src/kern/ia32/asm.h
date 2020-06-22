#ifndef ASM_SANITY_CHECK_H
#define ASM_SANITY_CHECK_H

#include "globalconfig.h"
#include "shortcut.h"

#ifdef __ASSEMBLER__

#include "tcboffset.h"

#define kdb_ke_asm(msg)		\
	int3			;\
	jmp	9f		;\
	.ascii	msg		;\
9:

#ifdef CONFIG_BIT64
# define PTR_VAL(x) .quad x
# define PTR_ALIGN 8
#else
# define PTR_VAL(x) .long x
# define PTR_ALIGN 4
#endif

.macro ASM_KEX ip:req, fixup:req, handler = 0
	.pushsection "__exc_table", "a?"
	.align PTR_ALIGN
	PTR_VAL(\ip)
	PTR_VAL(\fixup)
	PTR_VAL(\handler)
	.popsection
.endm

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

#else // __ASSEMBLY__

#ifdef CONFIG_BIT64
# define ASM_PTR_VAL(x) ".quad " #x
# define ASM_PTR_ALIGN "8"
#else
# define ASM_PTR_VAL(x) ".long " #x
# define ASM_PTR_ALIGN "4"
#endif

#define ASM_KEX(ip, fixup) \
	"\t.section __exc_table.%=, \"a?\"     \n" \
	"\t.align " ASM_PTR_ALIGN             "\n" \
	"\t" ASM_PTR_VAL(ip)                  "\n" \
	"\t" ASM_PTR_VAL(fixup)               "\n" \
	"\t" ASM_PTR_VAL(0)                   "\n" \
	"\t.previous                           \n"

#endif

#endif // ASM_SANITY_CHECK_H
