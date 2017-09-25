/* -*- asm -*- */

#include "tcboffset.h"
#include "config_tcbsize.h"

#define SYSCALL(name) .long sys_##name##_wrapper

/* Return frame offsets */
#define RF_XER	-36
#define RF_CTR	-32
#define RF_CR	-28
#define RF_SRR1	-24
#define RF_SRR0	-20
#define RF_ULR	-16
#define RF_USP	-12

.section .exceptcommon, "ax"

/*******************************************************************************
 * System-call table
 */
.global sys_call_table
sys_call_table:
  .long sys_kdb_ke         // +0x4 (== sys call binding offsets, not 0x0 since
  .long sys_kdb_ke         // +0x8  srr0 points to instr after 'sc', see
  SYSCALL(ipc)             // +0xC  system_call function below)
  .long sys_kdb_ke         // +0x10
  SYSCALL(invoke_debug)    // +0x14
  .long sys_kdb_ke         // +0x18
  .long sys_kdb_ke         // +0x1C
  .long sys_kdb_ke         // +0x20
  .long sys_kdb_ke         // +0x24
  .long sys_kdb_ke         // +0x28


/*******************************************************************************
 * Save volatile registers on return frame and system call frame.
 * Separate section, because we only have 256 bytes or 64 instructions space per
 * exception 
 */

.align 2
.global _save_volatile_gpr
_save_volatile_gpr:
  /* spill system-call frame */
  stmw		13,   -76(%r11) //save r31 - r13
  stwu		%r10, -80(%r11) //ip, r12, r11 already saved
  stwu		%r9 , -4(%r11)
  stwu		%r8 , -4(%r11)
  stwu		%r7 , -4(%r11)
  stwu		%r6 , -4(%r11)
  stwu		%r5 , -4(%r11)
  stwu		%r4 , -4(%r11)
  stwu		%r3 , -4(%r11)
  stwu		%r2 , -4(%r11)
  stwu		%r0 , -4(%r11)

  blr

.align 2
.global _save_volatile
_save_volatile:

  /* spill return frame */
  mfsrr0	%r12				//exception ip
  stwu		%r12, -4(%r11)
  mfsrr1	%r12				//msr flags
  stwu		%r12, -4(%r11)
  mfcr		%r12				//cr
  stwu		%r12, -4(%r11)
  mfctr		%r12				//ctr
  stwu		%r12, -4(%r11)
  mfxer		%r12				//xer
  stwu		%r12, -4(%r11)
  mfsrr0	%r12				//save ip in syscall frame
  stwu		%r12, -4(%r11)

  /* set KERNEL flags */
  lis		%r12, VAL__MSR__KERNEL@ha
  li		%r12, VAL__MSR__KERNEL@l
  mtmsr		%r12
  isync

  b		_save_volatile_gpr

.align 2
.global return_from_exception
return_from_exception:
  mr		%r12, %r1
  addi		%r12, %r12, (VAL__STACK__FRAME - VAL__ENTRY__FRAME)
  lwz		%r0,  0(%r12)
  lwzu		%r3,  8(%r12) //skip r2
  lwzu		%r4,  4(%r12)
  lwzu		%r5,  4(%r12)
  lwzu		%r6,  4(%r12)
  lwzu		%r7,  4(%r12)
  lwzu		%r8,  4(%r12)
  lwzu		%r9,  4(%r12)
  lwzu		%r10, 4(%r12)
  lmw		13,   4(%r12)			//load r13 - r31
  lwzu		%r11, 84(%r12)			//xer + skip ip register
  						//of syscall frame
  mtxer		%r11
  lwzu		%r11, 4(%r12)			//ctr
  mtctr		%r11
  lwzu		%r11, 4(%r12)			//cr
  mtcr		%r11
  lwzu		%r11, 4(%r12)			//srr1
  mtsrr1	%r11
  lwzu		%r11, 4(%r12)			//srr0
  mtsrr0	%r11
  blr

.macro finish_exception opcode
  lwzu		%r11, 4(%r12)			//ulr
  mtlr		%r11
  lwzu		%r1,  4(%r12)			//usp
  lwzu		%r11, 4(%r12)			//r11
  addi		%r12, %r12, 8 			//should be stack frame
  						//beginning
  mtsprg1	%r12				//save in stack-frame register
  lwz		%r12, -4(%r12)			//r12
  \opcode
.endm

.macro finish_rfi
  finish_exception rfi
.endm

.macro finish_blr
  finish_exception blr
.endm

.macro save_volatile
  mtsprg2	%r1				//tmp save user stack
  mtsprg3	%r12				//tmp save r12
  mfcr		%r1				//cr
  mfsrr1	%r12				//get srr1
  andi.		%r12, %r12, VAL__MSR__PR@l	//srr1 & Cpu::Msr_pr
  beq	1f					//if we are coming from kernel
  						//mode, don't load kernel sp
  mtcr		%r1				//restore cr
  mfsprg1	%r1				//kernel sp
  b		2f
1:
  mtcr		%r1
  mfsprg2	%r1
2:
  mfsprg3	%r12				//recover r12
  stw		%r12, -4(%r1)			//save r12
  mtsprg3	%r11				//tmp save r11
  mr		%r11, %r1			//save r1 in r11
  subi		%r1,  %r1, VAL__STACK__FRAME	//establish stack frame
  mfsprg3	%r12				//restore r11
  stwu		%r12, -8(%r11)			//save r11
  mfsprg2	%r12				//restore r1
  stwu		%r12, -4(%r11)			//save usp
  mflr		%r12
  stwu		%r12, -4(%r11)			//save ulr

  bl		_save_volatile
.endm

/*******************************************************************************
 * reset the Thread_cancel flag
 * r11, r12 are scrached, r12 contains the thread state
 */
.macro RESET_THREAD_CANCEL
  lis		%r11, (THREAD_BLOCK_SIZE - 1)@ha
  li		%r11, (THREAD_BLOCK_SIZE - 1)@l
  andc		%r11, %r1, %r11			//apply mask to r1
  lwz		%r12, OFS__THREAD__STATE(%r11)
  rlwinm	%r12, %r12, 0, 25, 23		//clear 0x80 (VAL__Thread_cancel) (or bit 24)
  stw		%r12, OFS__THREAD__STATE(%r11)
.endm

/*******************************************************************************
 * system call, entry calculations
 */
/* TODO: implement range check */
.align 2 
.global system_call
system_call:
  RESET_THREAD_CANCEL
  addi		%r12, %r1, VAL__STACK__FRAME	//Return frame
  lwz		%r11, RF_SRR0(%r12)		//srr0/ip
  lwz		%r10, RF_ULR(%r12)		//load ulr and overwrite srr0 on
  						//return frame
  stw		%r10, RF_SRR0(%r12)
  mr		%r12, %r11
  lis		%r11, sys_call_table@ha		//get offset in sys table
  li		%r11, sys_call_table@l
  sub		%r12, %r11, %r12
  lwz		%r12, 0(%r12)			//load address in sys table
  mtctr		%r12
  bctr

/*******************************************************************************
 * exception entries
 */
.section .except, "ax"

.global _except_system_reset
. = 0x100
_except_system_reset:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

.global _except_machine_check
. = 0x200
_except_machine_check:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

/* TODO cbass: add handler if page-fault handling is unsuccessfull */
.global _except_dsi
. = 0x300
_except_dsi:
	save_volatile
	mfdar	%r3                        //pfa-address
	mfdsisr	%r4                        //error codes
	mfsrr1	%r12
	andi.	%r12, %r12, VAL__MSR__PR@l //get privilege level
	or	%r4,  %r4, %r12            //set privilege level in error code
	mfsrr0	%r5                        //pc
	mfsprg1	%r6                        //Return_frame
	bl	pagefault_entry
	bl	return_from_exception
	finish_rfi

/* TODO cbass: add handler if page-fault handling is unsuccessfull */
.global _except_isi
. = 0x400
_except_isi:
	save_volatile
	mfsrr0  %r3 //pfa-address
	mfsrr1  %r4 //error codes
	mfsrr0  %r5 //pc = pfa
	mfsprg1 %r6 //Return_frame
	bl	pagefault_entry
	bl	return_from_exception
	finish_rfi

.global _except_interrupt
. = 0x500
_except_interrupt:
	save_volatile
	bl	irq_handler
	bl	return_from_exception
	finish_rfi

.global _except_align
. = 0x600
_except_align:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

.global _except_program
. = 0x700
_except_program:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

.global _except_float
. = 0x800
_except_float:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

/** 
 *decrementer (timer) exception 
 */
.global _except_decrementer
. = 0x900
_except_decrementer:
	save_volatile
	bl	timer_handler
	bl	return_from_exception
	finish_rfi

.global _except_system_call
. = 0xc00
_except_system_call:
	save_volatile
	bl	system_call
	bl	return_from_exception
	finish_rfi

.global _except_trace
. = 0xd00
_except_trace:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

.global _except_float_assist
. = 0xe00
_except_float_assist:
	save_volatile
	bl	except_notimpl
	bl	return_from_exception
	finish_rfi

// XXX: just a build fix...
.align 4
	.global leave_by_trigger_exception
leave_by_trigger_exception:
// XXX: end of just a build fix...

.text
	.global vcpu_resume
vcpu_resume:
// XXX: end of just a build fix...

/*******************************************************************************
 * kdebug entry
 */
.section .exceptcommon, "ax"

.macro DEBUGGER_ENTRY
	mtsprg3		%r12		//set srr1 to msr
	mfmsr		%r12
	mtsrr1		%r12
	mfsprg3		%r12
	save_volatile
	mflr		%r12		//error-code
	stwu		%r12, -4(%r11)
	lis		%r12,  0	//pfa
	li		%r12, -1
	stwu		%r12, -4(%r11)
	subi		%r1, %r1, 16	//adjust stack frame
	mr		%r3, %r11	//set Trap_state ptr
	
	bl		call_nested_trap_handler
	addi		%r1, %r1, 16
	bl		return_from_exception
	finish_blr
.endm

.align 2
.global kern_kdebug_entry
kern_kdebug_entry:
#ifdef CONFIG_JDB
	DEBUGGER_ENTRY
#else
	blr
#endif



