/* -*- c -*- */

	.globl	_start
_start:
	/* Clear the base pointer so that stack backtraces will work.  */
	xorl	%ebp,%ebp
	leal	_stack_end,%esp
	call	__main
	/* never returns */

	.bss
	.space	2048
_stack_end:
