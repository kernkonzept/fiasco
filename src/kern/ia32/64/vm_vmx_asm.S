/**
 * Function resume_vm_vmx, arguments:
 *  - rdi: Pointer to registers (layout specific...)
 */
	.p2align(4)
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
	mov	$0x6c14, %eax      // save rsp in vmcs
	vmwrite	%rsp, %rax
	mov	(%rsp), %rsp

	// load guest GP registers
	mov	  0(%rsp), %r15
	mov	  8(%rsp), %r14
	mov	 16(%rsp), %r13
	mov	 24(%rsp), %r12
	mov	 32(%rsp), %r11
	mov	 40(%rsp), %r10
	mov	 48(%rsp), %r9
	mov	 56(%rsp), %r8
	mov	 64(%rsp), %rdi
	mov	 72(%rsp), %rsi
	mov	 80(%rsp), %rbp
	mov	 96(%rsp), %rbx
	mov	104(%rsp), %rdx
	mov	112(%rsp), %rcx
	mov	120(%rsp), %rax

	vmresume
	jnz 1f
	vmlaunch
1:	// error path
	mov	$0x6c14, %rbx
	vmread	%rbx, %rsp
	mov	$1, %eax
	add	$8, %rsp        // pushed regs pointer
	pop	%rbp
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	ret

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
	ret

