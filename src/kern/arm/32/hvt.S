/* -*- asm -*- */

#include "globalconfig.h"
#include "config_tcbsize.h"
#include "tcboffset.h"
#include "asm_entry.h"

.arch_extension virt

/**
 * Reconfigure MPU for kernel space.
 *
 * Clobbers registers r0-r3.
 *
 * \param check   Test for target mode. Can be 0 if known to come from user
 *                space. Otherwise 1 to test for kernel re-entry.
 */
.macro mpu_kernel_enter check
#ifdef CONFIG_MPU
.if \check != 0
	mrs	lr, spsr
	and	r0, r0, #0x1f		@ Mask all but mode bits
	cmp	r0, #0x1a		@ Check if we're coming from hyp-mode
	beq	111f			@ Kernel re-entry
.endif
	bl	__mpu_kernel_enter
111:
#endif
.endm

/**
 * Reconfigure MPU for user space.
 *
 * Clobbers registers r1-r3, r12. Spares r0 because it's relevant for the vCPU
 * kernel mode exit path.
 *
 * \param check   Test for target mode. Can be 0 if known to return to user
 *                space. Otherwise 1 if the eret may return to kernel mode.
 * \param offset  Kernel stack offset from entry frame.
 */
.macro mpu_kernel_leave check, offset
#ifdef CONFIG_MPU
.if \check != 0
	ldr	r1, [sp, #RF(PSR, \offset)]	@ get spsr from stack
	and	r1, r1, #0x1f			@ Mask all but mode bits
	cmp	r1, #0x1a			@ Check if we're coming from hyp-mode
	beq	112f				@ Kernel re-entry
.endif
	bl	__mpu_kernel_leave
112:
#endif
.endm


.macro  switch_to_hyp_kernel
	sub	sp, sp, #RF_SIZE
	clrex
	str	lr, [sp, #RF(USR_LR, 0)]
	mrs	lr, ELR_hyp
	str	lr, [sp, #RF(PC, 0)]
	mrs	lr, spsr
	str	lr, [sp, #RF(PSR, 0)]
	mrs	lr, SP_usr
	str	lr, [sp, #RF(USR_SP, 0)]
.endm

.macro 	return_from_exception
	ldr	lr, [sp, #RF(PSR,0)]		@ Unstack SPSR
	add	sp, sp, #RF_SIZE		@ SP to top of stack
	msr	spsr_cfsx, lr			@ Load SPSR from kernel_lr
	ldr	lr, [sp, #RF(PC, -RF_SIZE)]
	msr	ELR_hyp, lr
	ldr	lr, [sp, #RF(USR_SP, -RF_SIZE)]
	msr	SP_usr, lr
	ldr	lr, [sp, #RF(USR_LR, -RF_SIZE)]
	eret
.endm

GEN_SYSCALL_TABLE

.macro disable_irqs
	cpsid	iaf
.endm

.align  4
.LCslowtrap_entry:	.word	slowtrap_entry

GEN_EXCEPTION_RETURN
GEN_IRET
GEN_VCPU_UPCALL OFS__THREAD__KERN_VCPU
GEN_LEAVE_BY_TRIGGER_EXCEPTION
GEN_LEAVE_AND_KILL_MYSELF
GEN_DEBUGGER_ENTRIES


.local hyp_irq_entry
hyp_irq_entry:
	switch_to_hyp_kernel

	stmdb	sp!, {r0 - r3, r12}   	@ Stack rest of user state
	align_and_save_sp r0
	mpu_kernel_enter 1
	mov	lr, pc
	ldr	pc, 1f
	ldr	sp, [sp]
	disable_irqs
	mpu_kernel_leave 1, 20
	ldmia	sp, {r0 - r3, r12}		@ Restore user state
	add	sp, sp, #20
	return_from_exception

	.global __irq_handler_irq
__irq_handler_irq:
1:	.word	irq_handler


.p2align 5
.globl hyp_vector_base
hyp_vector_base:
	nop				/* Not used	*/
	b	hyp_undef_entry		/* UNDEF	*/
	b	hyp_swi_entry		/* SWI		*/
	b	hyp_inst_abort_entry	/* IABORT	*/
	b	hyp_data_abort_entry	/* DABORT	*/
	b	hyp_trap_entry		/* HYP TRAP	*/
	b	hyp_irq_entry		/* IRQ		*/
	b	hyp_irq_entry		/* FIQ		*/

.macro HYP_FAULT err
	switch_to_hyp_kernel
	stmdb	sp!, {r0 - r12}		@ Stack rest of user state
	sub	sp, #(4 * 2)		@ dummy PFA and dummy error code
	mpu_kernel_enter 0
	mov	r0, #\err
	b	hyp_fault_call
.endm

.align 4
hyp_undef_entry:
	HYP_FAULT 0

hyp_swi_entry:
	HYP_FAULT 1

hyp_inst_abort_entry:
	HYP_FAULT 2

hyp_data_abort_entry:
	HYP_FAULT 3

hyp_fault_call:
	mov	r1, sp
	align_and_save_sp r2
	bl	hyp_mode_fault
	ldr	sp, [sp]
	add	sp, sp, #8
	disable_irqs
	mpu_kernel_leave 1, (13*4)
	ldmia	sp, {r0 - r12}
	add	sp, sp, #(13 * 4)
	return_from_exception

hyp_trap_entry:
	switch_to_hyp_kernel
	stmdb	sp!, {r0 - r12}   	@ Stack rest of user state
	mpu_kernel_enter 0
	add	r0, sp, #13*4		@ dummy PFA and dummy error code
	sub	sp, sp, #8
	mov	lr, pc
	ldr	pc, 1f
	add	sp, sp, #8
.global fast_ret_from_irq
.type fast_ret_from_irq, #function
fast_ret_from_irq:
	disable_irqs
	mpu_kernel_leave 1, (13*4)
	ldmia	sp, {r0 - r12}		@ Restore user state
	add	sp, sp, #13*4
	return_from_exception
1:	.word arm_esr_entry

# r0 = registers, r1 = temp stack
.global	vcpu_resume
.type vcpu_resume, #function
vcpu_resume:
#ifdef CONFIG_MPU
	// We cannot update sp before calling mpu_kernel_leave because
	// CONTEXT_OF does not work with a fully emtpy stack!
	mov	r4, r1
	mpu_kernel_leave 0, 0
	add	sp, r4, #RF_SIZE
#else
	add	sp, r1, #RF_SIZE
#endif
	add	r0, r0, #8			@ pfa, err
	ldr	r1, [r0, #RF(PSR, 13*4)]	@ Unstack SPSR
	msr	spsr_cfsx, r1
	ldr	lr, [r0, #RF(USR_LR, 13*4)]	@ load LR_usr from vcpu state

	ldr	r1, [r0, #RF(USR_SP, 13*4)]
	msr	sp_usr, r1

	ldr	r1, [r0, #RF(PC, 13*4)]
	msr	ELR_hyp, r1
	ldmia	r0, {r0-r12}
	eret

#ifdef CONFIG_MPU
.if OFS__THREAD__MPU_PRBAR2+4 != OFS__THREAD__MPU_PRLAR2
.error "_mpu_prbar2 and _mpu_prlar2 must be adjacent"
.endif

__mpu_kernel_enter:
	CONTEXT_OF r0, sp

	// restore regular heap region
	ldrd	r2, r3, [r0, #(OFS__THREAD__MPU_PRBAR2)]
	mcr	p15, 4, r2, c6, c9, 0	// write HPRBAR2
	mcr	p15, 4, r3, c6, c9, 1	// write HPRLAR2

	// disable user space ku_mem mappings
	ldr	r1, [r0, #(OFS__THREAD__KU_MEM_REGIONS)]
	mrc	p15, 4, r2, c6, c1, 1	// read HPRENR
	bic	r1, r2, r1
	mcr	p15, 4, r1, c6, c1, 1	// write back HPRENR

	// Technically we should execute an ISB to make sure the MPU updates
	// have settled. Practically, the relevant CPUs are in-order
	// architectures and do not need this. In the worst case a level 0
	// fault happens which is gracefully handled by
	// Thread::handle_hyp_mode_fault().
	bx	lr

__mpu_kernel_leave:
	CONTEXT_OF r1, sp

	// enable all user regions
	mrc	p15, 4, r3, c6, c1, 1		// read HPRENR
	ldr	r2, [r1, #(OFS__THREAD__KU_MEM_REGIONS)]
	orr	r2, r2, r3		// enable user ku_mem regions
	mcr	p15, 4, r2, c6, c1, 1	// write user HPRENR

	// constrain heap region to stack
	mrc	p15, 4, r2, c6, c9, 0	// read HPRBAR2
	mrc	p15, 4, r3, c6, c9, 1	// read HPRLAR2

	add	r12, r1, #((THREAD_BLOCK_SIZE-1) & ~0x3f)
	and	r2, r2, #0x3f
	and	r3, r3, #0x3f
	orr	r2, r2, r1
	orr	r3, r3, r12

	mcr	p15, 4, r2, c6, c9, 0	// write HPRBAR2
	mcr	p15, 4, r3, c6, c9, 1	// write HPRLAR2

	// The final eret will implicitly synchronize the MPU updates above.
	bx	lr
#endif
