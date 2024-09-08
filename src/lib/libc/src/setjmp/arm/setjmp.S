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
