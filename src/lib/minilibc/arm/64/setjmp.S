.type setjmp,function
.global setjmp
setjmp:
	stp	x19, x20, [x0, #(0<<4)]
	stp	x21, x22, [x0, #(1<<4)]
	stp	x23, x24, [x0, #(2<<4)]
	stp	x25, x26, [x0, #(3<<4)]
	stp	x27, x28, [x0, #(4<<4)]
	stp	x29, x30, [x0, #(5<<4)]
	mov	x2, sp
	str	x2, [x0, #(6<<4)]
	mov	x0, #0
	ret	x30

.type longjmp,function
.global longjmp
longjmp:
	ldp	x19, x20, [x0, #(0<<4)]
	ldp	x21, x22, [x0, #(1<<4)]
	ldp	x23, x24, [x0, #(2<<4)]
	ldp	x25, x26, [x0, #(3<<4)]
	ldp	x27, x28, [x0, #(4<<4)]
	ldp	x29, x30, [x0, #(5<<4)]
	ldr	x2, [x0, #(6<<4)]
	mov	sp, x2
	mov	x0, #1
	br	x30
