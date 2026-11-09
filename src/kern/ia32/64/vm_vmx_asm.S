#include <low_level.h>

/**
 * Function resume_vm_vmx, arguments:
 *  - rdi: Pointer to registers (layout specific...)
 */
	.p2align 4
	.globl resume_vm_vmx
	.globl vm_vmx_exit_vec
resume_vm_vmx:
	// save callee saved regs
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rbp

	push	%rdi               // store pointer to register struct
	mov	$0x6c14, %eax      // save RSP in Host RSP field (0x6c14)
	vmwrite	%rsp, %rax
	mov	%rdi, %rax

	// load guest GP registers, RSP keeps pointing to the kernel stack
	mov	  0(%rax), %r15
	mov	  8(%rax), %r14
	mov	 16(%rax), %r13
	mov	 24(%rax), %r12
	mov	 32(%rax), %r11
	mov	 40(%rax), %r10
	mov	 48(%rax), %r9
	mov	 56(%rax), %r8
	mov	 64(%rax), %rdi
	mov	 72(%rax), %rsi
	mov	 80(%rax), %rbp
	mov	 96(%rax), %rbx
	mov	104(%rax), %rdx
	mov	112(%rax), %rcx
	mov	120(%rax), %rax

	vmresume
	jnz	1f

	// check VM-Instruction Error field (0x4400) for
	// non-launched VMCS error (5)
	mov	$0x4400, %rbx
	vmread	%rbx, %rbx
	cmpl	$5, %ebx
	jne	1f

	mov	(%rsp), %rbx
	mov	 96(%rbx), %rbx
	vmlaunch

1:	// error path
	mov	$1, %eax
	add	$8, %rsp        // pushed regs pointer
	pop	%rbp
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	SAFE_RET

vm_vmx_exit_vec:
	push	%rsi
	mov	8(%rsp), %rsi     // get previously saved register struct pointer
	mov	%r15,  0(%rsi)
	mov	%r14,  8(%rsi)
	mov	%r13, 16(%rsi)
	mov	%r12, 24(%rsi)
	mov	%r11, 32(%rsi)
	mov	%r10, 40(%rsi)
	mov	%r9,  48(%rsi)
	mov	%r8,  56(%rsi)
	mov	%rdi, 64(%rsi)
	// skip rsi for now
	mov	%rbp, 80(%rsi)
	mov	%rbx, 96(%rsi)
	mov	%rdx,104(%rsi)
	mov	%rcx,112(%rsi)
	mov	%rax,120(%rsi)
	pop	%rcx
	mov	%rcx, 72(%rsi)

	add	$8, %rsp           // adjust stack after regs pointer push on stack

	// restore callee saved registers
	pop	%rbp
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx

	xor	%rax, %rax
	SAFE_RET
