// vim: ft=asm
#pragma once

#include "globalconfig.h"
#include "config_tcbsize.h"
#include "tcboffset.h"

/****************************
 * some handy definitions
 */
#define RF_SIZE      20
#define RF_PSR       16
#define RF_PC        12
#define RF_SVC_LR     8
#define RF_USR_LR     4
#define RF_USR_SP     0
#define RF(reg, offs) (RF_##reg + (offs))

#define GET_HSR(ec) (ec << 26)

/**********************************************************************
 * calculate the TCB address from a stack pointer
 */
.macro CONTEXT_OF reg, ptr
	bic	\reg, \ptr, #((THREAD_BLOCK_SIZE-1) & 0xff)
	bic	\reg, \reg, #((THREAD_BLOCK_SIZE-1) & 0xff00)
.endm

/**********************************************************************
 * Reset the thread cancel flag. 
 * Register r0 is scratched and contains the thread state afterwards
 */
.macro RESET_THREAD_CANCEL_AT tcb
	ldr 	r0, [\tcb, #(OFS__THREAD__STATE)]
	bic	r0, r0, #VAL__Thread_cancel
	str	r0, [\tcb, #(OFS__THREAD__STATE)]
.endm


/*****************************************************************************/
/* The syscall table stuff                                                   */
/*****************************************************************************/
.macro GEN_SYSCALL_TABLE
.align 4
.global sys_call_table
sys_call_table:
	.word sys_ipc_wrapper
	.word sys_arm_mem_op
.endm

.macro GEN_VCPU_UPCALL THREAD_VCPU
.align 4
.global leave_by_vcpu_upcall;
.type leave_by_vcpu_upcall, #function

#define OFS__VCPU_STATE__RF (VAL__SIZEOF_TRAP_STATE - RF_SIZE + OFS__VCPU_STATE__TREX)

/**
 * Return to user space in vCPU kernel mode.
 *
 * This is called as continuation (triggered by
 * Context::vcpu_save_state_and_upcall()) after the user space registers have
 * been restored. We just have to make sure that no registers are clobbered.
 */
leave_by_vcpu_upcall:
	sub 	sp, sp, #(RF_SIZE + 3*4)   @ restore old return frame + 3 regs
        /* save r0, r1, r2 for scratch registers */
	stmia 	sp, {r0 - r2}

	/* restore original IP */
	CONTEXT_OF r0, sp
	ldr	r1, [r0, #(\THREAD_VCPU)]
	add	r1, r1, #(OFS__VCPU_STATE__RF)

	/* r0 = current() */
	/* r1 = &vcpu_state->ts.r[13] */

	ldr	r2, [r0, #(OFS__THREAD__EXCEPTION_IP)]
	str	r2, [r1, #RF(PC, 0)]
	ldr	r2, [r0, #(OFS__THREAD__EXCEPTION_PSR)]
	str	r2, [r1, #RF(PSR, 0)]

	ldr	r2, [sp, #RF(USR_LR, 3*4)]
	str	r2, [r1, #RF(USR_LR, 0)]

	ldr	r2, [sp, #RF(USR_SP, 3*4)]
	str	r2, [r1, #RF(USR_SP, 0)]

	/* Reset continuation */
	mov	r2, #~0
	str	r2, [r0, #(OFS__THREAD__EXCEPTION_IP)]

	/* Save all, except scratch registers to vCPU state */
	stmdb	r1!, {r3-r12}

	/* r1 = &vcpu_state->ts.r[3] */

	/* Store scratch registers saved previously to vCPU state */
	ldm	sp, {r3, r12, lr}     @ r3 = r0, r12 = r1, lr = r2
	stmdb	r1, {r3, r12, lr}

	add	sp, sp, #(3*4)			      @ now sp points at return frame
	sub	r1, r1, #(OFS__VCPU_STATE__RF - 10*4) @ now r1 points to the VCPU STATE again

	bl	current_prepare_vcpu_return_to_kernel

	/* r0 = vCPU user pointer */

	/*
         * We only need to clear our scratch registers (r1-r3 and r12). All
         * other registers are callee saved and have been preserved across the
         * current_prepare_vcpu_return_to_kernel call. There is no need to
         * clear lr because it is either restored by __iret (cpu_virt) or is a
         * banked register (!cpu_virt).
	 */
	mov	r1, #0
	mov	r2, #0
	mov	r3, #0
	mov	r12, #0
	b	__iret_safe

.endm

/**********************************************************************
	kdebug entry
 **********************************************************************/

.macro DEBUGGER_ENTRY type
#ifdef CONFIG_JDB
	str	sp, [sp, #(RF(USR_SP, -RF_SIZE))] @ save r[13]
	sub	sp, sp, #(RF_SIZE)

	str	lr, [sp, #RF(SVC_LR, 0)]
	str	lr, [sp, #RF(PC, 0)]
        mrs	lr, cpsr
	str	lr, [sp, #RF(PSR, 0)]

	stmdb	sp!, {r0 - r12}
	mov	r0, #-1			@ pfa
	mov	r1, #GET_HSR(0x33)	@ err
	orr	r1, #\type		@ + type
	stmdb	sp!, {r0, r1}

	mov	r0, sp
	adr	lr, 1f
	ldr	pc, 3f

1:
	add	sp, sp, #8		@ pfa, err
	ldmia	sp!, {r0 - r12}
	ldr	lr, [sp, #RF(PSR, 0)]
	msr	cpsr, lr
	ldr	lr, [sp, #RF(SVC_LR, 0)]

	ldr	sp, [sp, #(RF(USR_SP, 0))]
	mov	pc, lr

3:	.word call_nested_trap_handler
#else
	mov	pc, lr
#endif
.endm


.macro GEN_DEBUGGER_ENTRIES
	.global	kern_kdebug_cstr_entry
	.type kern_kdebug_cstr_entry, #function
	.align 4
kern_kdebug_cstr_entry:
	DEBUGGER_ENTRY 0

	.global	kern_kdebug_nstr_entry
	.type kern_kdebug_nstr_entry, #function
	.align 4
kern_kdebug_nstr_entry:
	DEBUGGER_ENTRY 1

	.global	kern_kdebug_sequence_entry
	.type kern_kdebug_sequence_entry, #function
	.align 4
kern_kdebug_sequence_entry:
	DEBUGGER_ENTRY 2

#ifdef CONFIG_MP
	.section ".text"
	.global	kern_kdebug_ipi_entry
	.type kern_kdebug_ipi_entry, #function
	.align 4
kern_kdebug_ipi_entry:
	DEBUGGER_ENTRY 3
	.previous
#endif

.endm

.macro align_and_save_sp orig_sp
	mov 	\orig_sp, sp
	tst	sp, #4
	subeq	sp, sp, #8
	subne	sp, sp, #4
	str	\orig_sp, [sp]
.endm

.macro 	enter_slowtrap_w_stack errorcode, ec2 = 0
	mov	r1, #\errorcode
        .if \ec2 != 0
           orr r1, r1, #\ec2
        .endif
	stmdb	sp!, {r0, r1}
	align_and_save_sp r0
	adr	lr, exception_return
	ldr	pc, .LCslowtrap_entry
.endm

.macro GEN_EXCEPTION_RETURN
	.global __return_from_user_invoke
	.type __return_from_user_invoke, #function
exception_return:
	disable_irqs
	ldr	sp, [sp]
__return_from_user_invoke:
	add	sp, sp, #8 // pfa, err
	ldmia	sp!, {r0 - r12}
	return_from_exception
.endm

.macro GEN_IRET
	.global __iret
	.type __iret, #function
__iret:
	/*
	 * Clear all registers before returning to vCPU entry handler. Spare r0
	 * because it's supposed to hold the vCPU user pointer. Also lr does
	 * not need to be cleared because it's covered by the exception return.
	 */
	mov	r1, #0
	mov	r2, #0
	mov	r3, #0
	mov	r4, #0
	mov	r5, #0
	mov	r6, #0
	mov	r7, #0
	mov	r8, #0
	mov	r9, #0
	mov	r10, #0
	mov	r11, #0
	mov	r12, #0
__iret_safe:
	return_from_exception
.endm

.macro GEN_LEAVE_BY_TRIGGER_EXCEPTION
.align 4
.global leave_by_trigger_exception
.type leave_by_trigger_exception, #function

leave_by_trigger_exception:
	sub 	sp, sp, #RF_SIZE   @ restore old return frame
	stmdb 	sp!, {r0 - r12}

	/* restore original IP */
	CONTEXT_OF r1, sp
	ldr 	r0, [r1, #(OFS__THREAD__EXCEPTION_IP)]
	str	r0, [sp, #RF(PC, 13*4)]

	ldr	r0, [r1, #(OFS__THREAD__EXCEPTION_PSR)]
	str	r0, [sp, #RF(PSR, 13*4)]

	mov     r0, #~0
	str	r0, [r1, #(OFS__THREAD__EXCEPTION_IP)]

	enter_slowtrap_w_stack GET_HSR(0x3e)
.endm

.macro GEN_LEAVE_AND_KILL_MYSELF
.align 4
.global leave_and_kill_myself
.type leave_and_kill_myself, #function

leave_and_kill_myself:
        // make space for a dummy Return_frame accessible by the callee
        sub     sp, sp, #RF_SIZE
        bl      thread_do_leave_and_kill_myself
        // does never return
.endm
