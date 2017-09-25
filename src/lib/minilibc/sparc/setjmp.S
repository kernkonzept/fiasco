#include <setjmp.h>

.text
.weak setjmp
.type setjmp,function
setjmp:
.global __setjmp
.type __setjmp,function
.align 2
__setjmp:
	ta 1

.text
.global longjmp
.type longjmp,function
.align 2
longjmp:
	ta 1
