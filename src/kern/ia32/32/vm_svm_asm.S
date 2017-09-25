/**
 * Function resume_vm_svm, arguments:
 *  - eax: physical VMCB address
 *  - edx: Pointer to registers (layout specific...)
 *
 * Note: To be called with interrupts disabled!
 */

	.p2align(4)
	.globl	resume_vm_svm
resume_vm_svm:

	// save callee saved regs
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ebp

	pushl %edx              // store pointer to register struct

	clgi			// super CLI

	sti
	mov 0x10(%edx), %edi
	mov 0x14(%edx), %esi
	mov 0x18(%edx), %ebp
	mov 0x20(%edx), %ebx
	mov 0x28(%edx), %ecx
	mov 0x24(%edx), %edx
	// EAX is in the vmcb

	// TODO: Debugregs etc.

	// eax used as implicit operand for vm* instructions
	vmload
	vmrun
	vmsave

	pushl %edx
	movl 4(%esp), %edx     // get previously saved register struct pointer

	movl %ecx, 0x28(%edx)
	movl %ebx, 0x20(%edx)
	movl %ebp, 0x18(%edx)
	movl %esi, 0x14(%edx)
	movl %edi, 0x10(%edx)

	movl (%esp), %ecx      // guest EDX to ECX
	movl %ecx, 0x24(%edx)

	// TODO: restore task register, clear busy flag

	cli
	stgi

	addl $8, %esp         // adjust stack after two pushs

	// restore callee saved registers
	popl %ebp
	popl %ebx
	popl %esi
	popl %edi

	ret
