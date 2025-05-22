/* -*- c -*- */

#include "globalconfig.h"

	.globl	_boot_start
	.p2align 4
_boot_start:
	lea	_stack,%rsp
	push	$0			/* no return address */
	jmp	bootstrap

	.p2align 2, 0x90
	.long   0x1BADB002              /* magic */
	.long   0x00000000              /* feature flags */
	.long   0 - 0x1BADB002

#ifdef CONFIG_INTEL_ITS_MITIGATION

	/* Provide these dummy thunks for the boot stage because the compiler
         * generates references to them and we use the same object code during
         * boot and post-boot. These are not performance-critical and we don't
         * need to care for alignment. */

	.globl __x86_return_thunk
__x86_return_thunk:
	ret

.macro IA32_INDIRECT_THUNK reg
	.section ".text.__x86_indirect_thunk_\reg", "ax", @progbits
	.globl	__x86_indirect_thunk_\reg
__x86_indirect_thunk_\reg:
	jmp	*%\reg
.endm

	IA32_INDIRECT_THUNK rax
	IA32_INDIRECT_THUNK rbx
	IA32_INDIRECT_THUNK rcx
	IA32_INDIRECT_THUNK rdx
	IA32_INDIRECT_THUNK rbp
	IA32_INDIRECT_THUNK rsp
	IA32_INDIRECT_THUNK rsi
	IA32_INDIRECT_THUNK rdi
	IA32_INDIRECT_THUNK r8
	IA32_INDIRECT_THUNK r9
	IA32_INDIRECT_THUNK r10
	IA32_INDIRECT_THUNK r11
	IA32_INDIRECT_THUNK r12
	IA32_INDIRECT_THUNK r13
	IA32_INDIRECT_THUNK r14
	IA32_INDIRECT_THUNK r15

#endif // CONFIG_INTEL_ITS_MITIGATION

	.bss
	.align	16
	.space	4096
_stack:

