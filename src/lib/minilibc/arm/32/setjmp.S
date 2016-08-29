#include <setjmp.h>
	
.text
.weak setjmp
.type setjmp,function
setjmp:
.global __setjmp
.type __setjmp,function
__setjmp:
	@mov	r1, #0
	stmia	r0, {r4-r11, sp, lr}
	mov	r0, #0 /* skipped on longjmp */
	mov	pc, lr

.text
.global longjmp
.type longjmp,function
longjmp:
	mov	ip, r0
	movs	r0, r1
	moveq	r0, #1
	ldmia   ip, {r4-r11, sp, pc}
