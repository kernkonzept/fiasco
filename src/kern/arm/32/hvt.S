/* -*- asm -*- */

#include "globalconfig.h"
#include "config_tcbsize.h"
#include "tcboffset.h"
#include "asm_entry.h"

.arch_extension virt

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
#ifdef CONFIG_MPU
	stmdb	sp!, {r0 - r2, lr}
	bl	__mpu_kernel_enter
	ldmia	sp!, {r0 - r2, lr}
#endif
.endm

.macro 	return_from_exception
#ifdef CONFIG_MPU
	stmdb	sp!, {r0 - r3, lr}
	bl	__mpu_kernel_leave
	ldmia	sp!, {r0 - r3, lr}
#endif
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

.macro LOAD_USR_SP vcpu_ptr
	ldr r0, [\vcpu_ptr, #OFS__VCPU_STATE__ENTRY_SP]
	str r0, [sp, #RF(USR_SP, 0)]
.endm

.macro LOAD_USR_VCPU reg, kvcpu, thread
	ldr \reg, [\thread, #(OFS__THREAD__USER_VCPU)]
.endm

.align  4
.LCslowtrap_entry:	.word	slowtrap_entry

GEN_EXCEPTION_RETURN
GEN_VCPU_UPCALL OFS__THREAD__KERN_VCPU, LOAD_USR_SP, LOAD_USR_VCPU, USER_MODE=0x1f
GEN_LEAVE_BY_TRIGGER_EXCEPTION
GEN_LEAVE_AND_KILL_MYSELF
GEN_DEBUGGER_ENTRIES


.local hyp_irq_entry
hyp_irq_entry:
	switch_to_hyp_kernel

	stmdb	sp!, {r0 - r3, r12}   	@ Stack rest of user state
	align_and_save_sp r0
	mov	lr, pc
	ldr	pc, 1f
	ldr	sp, [sp]
	ldmia	sp, {r0 - r3, r12}		@ Restore user state
	disable_irqs
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
	sub	sp, #(4 * 2)
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
	b	hyp_mode_fault

hyp_trap_entry:
	switch_to_hyp_kernel

	stmdb	sp!, {r0 - r12}   	@ Stack rest of user state
	add	r0, sp, #13*4
	sub	sp, sp, #8
	mov	lr, pc
	ldr	pc, 1f
	add	sp, sp, #8
.global fast_ret_from_irq
fast_ret_from_irq:
	ldmia	sp, {r0 - r12}		@ Restore user state
	disable_irqs
	add	sp, sp, #(13*4)
GEN_IRET
1:	.word arm_esr_entry

# r0 = registers, r1 = temp stack
.global	vcpu_resume
vcpu_resume:
#ifdef CONFIG_MPU
	stmdb	sp!, {r0 - r3}
	bl	__mpu_kernel_leave_nocheck
	ldmia	sp!, {r0 - r3}
#endif
	add	sp, r1, #RF_SIZE
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
__mpu_kernel_enter:
	ldr	r0, [sp, #RF(PSR, 4*4)]	@ get spsr from stack
	and	r0, r0, #0x1f		@ Mask all but mode bits
	cmp	r0, #0x1a		@ Check if we're coming from hyp-mode
	beq	1f			@ Kernel re-entry

	CONTEXT_OF r0, sp

	// We remove the user mappings. Make sure all accesses in-flight are
	// retired with the old MPU settings to prevent false faults.
	dfb

	// disable user space ku_mem mappings
	ldr	r1, [r0, #(OFS__THREAD__MPU_USR_REGIONS)]
	mrc	p15, 4, r2, c6, c1, 1	// read HPRENR
	bic	r1, r2, r1
	mcr	p15, 4, r1, c6, c1, 1	// write back HPRENR

	// make KIP writable
	mrc	p15, 4, r2, c6, c8, 4	// read HPRBAR1
	bfc	r2, #1, #2		// AP[2:1] = b00 -> RW@EL1
	mcr	p15, 4, r2, c6, c8, 4	// write back HPRBAR1

	// restore regular heap region
	ldr	r1, [r0, #(OFS__THREAD__MPU_PRBAR2)]
	ldr	r2, [r0, #(OFS__THREAD__MPU_PRLAR2)]
	mcr	p15, 4, r1, c6, c9, 0	// write HPRBAR2
	mcr	p15, 4, r2, c6, c9, 1	// write HPRLAR2

	isb

1:
	bx	lr

__mpu_kernel_leave:
	ldr	r0, [sp, #RF(PSR, 5*4)]	@ get spsr from stack
	and	r0, r0, #0x1f		@ Mask all but mode bits
	cmp	r0, #0x1a		@ Check if we're coming from hyp-mode
	beq	1f			@ Kernel re-entry

__mpu_kernel_leave_nocheck:
	CONTEXT_OF r0, sp

	// Make sure all in-kernel memory accesses are retired before we change
	// the MPU.
	dsb

	// constrain heap region to stack
	mrc	p15, 4, r1, c6, c9, 0	// read HPRBAR2
	mrc	p15, 4, r2, c6, c9, 1	// read HPRLAR2
	add	r3, r0, #((THREAD_BLOCK_SIZE-1) & ~0x3f)

	and	r1, r1, #0x3f
	and	r2, r2, #0x3f
	orr	r1, r1, r0
	orr	r3, r3, r2

	mcr	p15, 4, r1, c6, c9, 0	// write HPRBAR2
	mcr	p15, 4, r3, c6, c9, 1	// write back HPRLAR2

	mrc	p15, 4, r2, c6, c1, 1	// read HPRENR
	ldr	r1, [r0, #(OFS__THREAD__MPU_USR_REGIONS)]
	orr	r1, r1, r2		// enable user ku_mem regions

	mrc	p15, 4, r2, c6, c8, 4	// read HPRBAR1

	// make KIP read only
	orr	r2, r2, #(3 << 1)	// AP[2:1] = b11 -> RO@EL2/1/0

	// enable all user regions, disable peripheral access
	mcr	p15, 4, r2, c6, c8, 4	// write back HPRBAR1
	mcr	p15, 4, r1, c6, c1, 1	// write user HPRENR

1:
	bx	lr
#endif
