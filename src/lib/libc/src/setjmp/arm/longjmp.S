.text
.global longjmp
.type longjmp,function
longjmp:
	mov	ip, r0
	movs	r0, r1
	moveq	r0, #1
	ldmia   ip, {r4-r11, sp, pc}
