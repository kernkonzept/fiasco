#include <setjmp.h>

.text
.weak setjmp
.type setjmp,@function
setjmp:
.globl __setjmp
.type __setjmp,@function
__setjmp:
	movl  %ebx, (JB_BX*4)(%eax)
	movl  %esi, (JB_SI*4)(%eax)
	movl  %edi, (JB_DI*4)(%eax)
	movl  %ebp, (JB_BP*4)(%eax)
	leal  4(%esp), %edx	/* Save SP as it will be after we return. */
	movl  0(%esp), %ecx	/* Save PC we are returning to now. */
	movl  %edx, (JB_SP*4)(%eax)
	movl  %ecx, (JB_PC*4)(%eax)
	xor   %eax,%eax
	ret
.size __setjmp,.-__setjmp;



.text
.global longjmp
.type longjmp,@function
longjmp:
                                /* eax = User's jmp_buf */
                                /* edx = Second argument is return value. */
        movl  (JB_PC*4)(%eax), %ecx
        movl  (JB_BX*4)(%eax), %ebx
        movl  (JB_SI*4)(%eax), %esi
        movl  (JB_DI*4)(%eax), %edi
        movl  (JB_BP*4)(%eax), %ebp
        movl  (JB_SP*4)(%eax), %esp
	movl  %edx, %eax
	/* From the longjmp manpage: ``If longjmp  is  invoked  with  a
	 * second argument of 0, 1 will be returned instead.'' */
	cmpl  $1, %eax
	adcl  $0, %eax
        /* Jump to saved PC.  */
        jmp   *%ecx
.size longjmp,.-longjmp;
