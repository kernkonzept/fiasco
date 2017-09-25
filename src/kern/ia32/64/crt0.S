/* -*- c -*- */

	.globl	_start
_start:
        /* Clear the base pointer so that stack backtraces will work.  */
        xor    %rbp,%rbp
	lea	_stack_end,%rsp
	call	__main
        /* never returns */

	.bss
	.space	2048
_stack_end:
