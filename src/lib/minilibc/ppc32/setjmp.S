#include <setjmp.h>

.text
.weak setjmp
.type setjmp,function
setjmp:
.global __setjmp
.type __setjmp,function
.align 2
__setjmp:
	stw	%r1, 0(%r3)
	stw	%r2, 4(%r3)
	mflr	%r0 		/* link register */
	stw	%r0, 8(%r3)
	mfcr	%r0		/* condition register */
	stw	%r0, 12(%r3)
	stmw	%r14,16(%r3)	/*XXX: r14 - r31 may be slow */
	xor	%r3, %r3, %r3	/* return 0 */
	blr

.text
.global longjmp
.type longjmp,function
.align 2
longjmp:
	lwz	%r1, 0(%r3)
	lwz	%r2, 4(%r3)
	lwz	%r0, 8(%r3)
	mtlr	%r0		/* link register */
	lwz	%r0, 12(%r3)
	mfcr	%r0		/* condition register */
	lmw	%r14,16(%r3)
	mr	%r3, %r4	/* return val in r4 */
	blr			/* branch to link register */
