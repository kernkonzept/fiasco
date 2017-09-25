	.p2align(4)
	.globl	resume_vm_svm
resume_vm_svm:
	
	// save callee saved regs
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rbp

	mov 	%rdi, %rax	// first arg goes to rax for vm*
	push	%rsi		// store pointer to register struct

	clgi

	sti
	nop

	// restore guest GP regs
	mov	  0(%rsi), %r15
	mov	  8(%rsi), %r14
	mov	 16(%rsi), %r13
	mov	 24(%rsi), %r12
	mov	 32(%rsi), %r11
	mov	 40(%rsi), %r10
	mov	 48(%rsi), %r9
	mov	 56(%rsi), %r8
	mov	 64(%rsi), %rdi
	// skip rsi for now
	mov	 80(%rsi), %rbp
	mov	 96(%rsi), %rbx
	mov	104(%rsi), %rdx
	mov	112(%rsi), %rcx
	mov	 72(%rsi), %rsi

	// TODO: Debugregs

	// rax used as implicit operand for vm* instructions
	vmload
	vmrun
	vmsave

	push	%rsi
	mov	8(%rsp), %rsi	// get previously saved register struct pointer

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
	mov	(%rsp), %rcx
	mov	%rcx, 72(%rsi)

	cli
	nop
	stgi

	add	$16, %rsp
	
	// restore callee saved registers
	pop	%rbp
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx

	ret
