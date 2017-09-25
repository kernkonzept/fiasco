/* -*- c -*- */

	.globl	_boot_start
	.p2align 4
_boot_start:
	lea	_stack,%rsp
	push	$0			/* no return address */
	jmp	bootstrap

        .align  4

	.p2align(2), 0x90
	.long   0x1BADB002              /* magic */
	.long   0x00000000              /* feature flags */
	.long   0 - 0x1BADB002

	.bss
	.align	8
	.space	4096
_stack:

