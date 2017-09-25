#include <setjmp.h>

.text
.weak setjmp
.type setjmp,@function
setjmp:
.globl __setjmp
.type __setjmp,@function
__setjmp:
	//mov 8(%rsp), %rax

	/* Save registers.  */
	mov %rbx, (JB_BX*8)(%rdi)
	mov %rbp, (JB_BP*8)(%rdi)
	mov %r8,  (JB_R8*8)(%rdi)
	mov %r9,  (JB_R9*8)(%rdi)
	mov %r10,  (JB_R10*8)(%rdi)
	mov %r11,  (JB_R11*8)(%rdi)
	mov %r12,  (JB_R12*8)(%rdi)
	mov %r13,  (JB_R13*8)(%rdi)
	mov %r14,  (JB_R14*8)(%rdi)
	mov %r15,  (JB_R15*8)(%rdi)
	lea 8(%rsp), %rdx	/* Save SP as it will be after we return.  */
	mov 0(%rsp), %rcx	/* Save PC we are returning to now.  */
	mov %rdx, (JB_SP*8)(%rdi)
	mov %rcx, (JB_PC*8)(%rdi)
	xor %rax,%rax
	ret
	
.size __setjmp,.-__setjmp;



.text
.global longjmp
.type longjmp,@function
longjmp:
        mov %rdi, %rcx      /* First argument is jump buffer. */
        mov %rsi, %rax      /* Second argument is return value. */
        /* Save the return address now.  */
        mov (JB_PC*8)(%rcx), %rdx
        /* Restore registers.  */
	mov (JB_R8*8)(%rcx), %r8
	mov (JB_R9*8)(%rcx), %r9
	mov (JB_R10*8)(%rcx), %r10
	mov (JB_R11*8)(%rcx), %r11
	mov (JB_R12*8)(%rcx), %r12
	mov (JB_R13*8)(%rcx), %r13
	mov (JB_R14*8)(%rcx), %r14
	mov (JB_R15*8)(%rcx), %r15
        mov (JB_BX*8)(%rcx), %rbx
        mov (JB_BP*8)(%rcx), %rbp
        mov (JB_SP*8)(%rcx), %rsp
	or  %rax, %rax
	jz   1f
        /* Jump to saved PC.  */
        jmp  *%rdx

	/* From the longjmp manpage: ``If longjmp  is  invoked  with  a
	 * second argument of 0, 1 will be returned instead.'' */
1:	mov $1, %rax
	jmp  *%rdx
.size longjmp,.-longjmp;
