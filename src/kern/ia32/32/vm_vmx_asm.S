/**
 * Function resume_vm_vmx, arguments:
 *  - eax: Pointer to registers (layout specific...)
 */
	.p2align(4)
	.globl resume_vm_vmx
	.globl vm_vmx_exit_vec
resume_vm_vmx:
	// save callee saved regs
	push	%edi
	push	%esi
	push	%ebx
	push	%ebp
	push	%ds
	push	%es
	push	%gs
	push	%fs

	push	%eax	        // store pointer to register struct
	mov	$0x6c14, %eax	// save esp in vmcs
	vmwrite	%esp, %eax
	mov	(%esp), %esp

	// load guest GPs
	mov	 16(%esp), %edi
	mov	 20(%esp), %esi
	mov	 24(%esp), %ebp
	mov	 32(%esp), %ebx
	mov	 36(%esp), %edx
	mov	 40(%esp), %ecx
	mov	 44(%esp), %eax

	vmresume
	jnz	1f
	vmlaunch
1: // error path
	mov	$0x6c14, %ebx  // restore stack pointer
	vmread	%ebx, %esp
	mov	$1, %eax       // flag error condition for the caller
	add	$20, %esp      // pushed regs pointer and gs+fs+ds+es
	pop	%ebp
	pop	%ebx
	pop	%esi
	pop	%edi
	ret

vm_vmx_exit_vec:
	push	%eax
	mov	4(%esp), %eax   // get previously saved register struct pointer

	// eax is saved below
	mov	%edi, 16(%eax)
	mov	%esi, 20(%eax)
	mov	%ebp, 24(%eax)
	mov	%ebx, 32(%eax)
	mov	%edx, 36(%eax)
	mov	%ecx, 40(%eax)
	pop	%ecx            // guest EAX to ECX
	mov	%ecx, 44(%eax)

	add	$4, %esp	// adjust stack after regs pointer push on stack
	
	// restore callee saved registers
	pop	%fs
	pop	%gs
	pop	%es
	pop	%ds
	pop	%ebp
	pop	%ebx
	pop	%esi
	pop	%edi

	xor	%eax, %eax
	ret
