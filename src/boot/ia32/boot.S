/* -*- c -*- */

	.globl	_boot_start
_boot_start:
	leal	_stack,%esp
	pushl	$0			/* no return address */
	jmp	bootstrap

        .align  4, 0x90
	.long   0x1BADB002              /* magic */
	.long   0x00000000              /* feature flags */
	.long   0 - 0x1BADB002

	.bss
	.space	1024
_stack:

