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
	.word 0
	.word 0
	.word sys_ipc_wrapper
	.word sys_arm_mem_op
.endm

.macro GEN_VCPU_UPCALL THREAD_VCPU, LOAD_USR_SP, LOAD_USR_VCPU
.align 4
.global leave_by_vcpu_upcall;

#define OFS__VCPU_STATE__RF (VAL__SIZEOF_TRAP_STATE - RF_SIZE + OFS__VCPU_STATE__TREX)

leave_by_vcpu_upcall:
	sub 	sp, sp, #(RF_SIZE + 3*4)   @ restore old return frame
        /* save r0, r1, r2 for scratch registers */
	stmia 	sp, {r0 - r2}

	/* restore original IP */
	CONTEXT_OF r1, sp
	ldr	r2, [r1, #(\THREAD_VCPU)]
	add	r2, r2, #(OFS__VCPU_STATE__RF)

	/* r1 = current() */
	/* r2 = &vcpu_state->ts.r[13] */

	ldr 	r0, [r1, #(OFS__THREAD__EXCEPTION_IP)]
	str	r0, [r2, #RF(PC, 0)]
	ldr	r0, [r1, #(OFS__THREAD__EXCEPTION_PSR)]
	str	r0, [r2, #RF(PSR, 0)]
	bic	r0, #0x2f // force ARM mode
	orr   r0, #0x10
	str	r0, [sp, #RF(PSR, 3*4)]

        ldr	r0, [sp, #RF(USR_LR, 3*4)]
        str	r0, [r2, #RF(USR_LR, 0)]

        ldr	r0, [sp, #RF(USR_SP, 3*4)]
        str	r0, [r2, #RF(USR_SP, 0)]

	mov     r0, #~0
	str	r0, [r1, #(OFS__THREAD__EXCEPTION_IP)]


	stmdb   r2, {r3-r12}

	/* Restore scratch registers saved previously */
	ldr	r0, [sp, #8]
	str	r0, [r2, #-44]

	ldr	r0, [sp, #4]
	str	r0, [r2, #-48]

	ldr	r0, [sp]
	str	r0, [r2, #-52]
        sub     r2, r2, #(OFS__VCPU_STATE__RF)     @ now r2 points to the VCPU STATE again

	add	sp, sp, #(3*4)

        \LOAD_USR_SP r2

	ldr	r0, [r2, #(OFS__VCPU_STATE__ENTRY_IP)]

	str	r0, [sp, #RF(PC, 0)]
        \LOAD_USR_VCPU r0, r2, r1
	b	__iret

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
	.global	kern_kdebug_entry
	.align 4
kern_kdebug_entry:
	DEBUGGER_ENTRY 0

	.global	kern_kdebug_sequence_entry
	.align 4
kern_kdebug_sequence_entry:
	DEBUGGER_ENTRY 1


#ifdef CONFIG_MP
	.section ".text"
	.global	kern_kdebug_ipi_entry
	.align 4
kern_kdebug_ipi_entry:
	DEBUGGER_ENTRY 2
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
__iret:
	return_from_exception
.endm

.macro GEN_LEAVE_BY_TRIGGER_EXCEPTION
.align 4
.global leave_by_trigger_exception

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
