
#include "tcboffset.h"

	.section ".initcall.data","aw"

sys_call_invoke:
	int	$0x30
	ret
sys_call_invoke_end:

sys_call_debugger:
	int	$0x32
	ret
sys_call_debugger_end:

sys_call_se_invoke:
	pop	%ebx
	mov	%esp, %ebp
	sysenter
/*
	0:
	push	%ecx
	subl	$4,%esp
	pushl	$0x1b
	pushl	$(1f-0b+VAL__MEM_LAYOUT__SYSCALLS)
	mov	%esp,%ecx
	sysenter
	mov	%ebp,%edx
	1:
	ret */
sys_call_se_invoke_end:

	.global	sys_call_invoke
	.global	sys_call_invoke_end
	.global	sys_call_debugger
	.global	sys_call_debugger_end
	.global	sys_call_se_invoke
	.global	sys_call_se_invoke_end

	.previous
