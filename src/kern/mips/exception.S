/* -*- asm -*- */

#include "asm_mips.h"
#include "asm_regs.h"
#include "asm_mips_regs.h"
#include "alternatives_asm.h"
#include "globalconfig.h"
#include "tcboffset.h"

#if defined CONFIG_CONTEXT_8K
#define THREAD_BLOCK_SHIFT (13)
#elif defined CONFIG_CONTEXT_4K
#define THREAD_BLOCK_SHIFT (12)
#else
#define THREAD_BLOCK_SHIFT (10)
#endif

#define COP0_SYNC   ehb

#define TS_FRAME (40 * ASM_WORD_BYTES) /* size of Trap_state */

#define TS_REG(x, offs)    (((x + 2) * ASM_WORD_BYTES) + offs)
#define TS_CAUSE(offs)     ((37 * ASM_WORD_BYTES) + offs)
#define TS_HI(offs)        ((34 * ASM_WORD_BYTES) + offs)
#define TS_LO(offs)        ((35 * ASM_WORD_BYTES) + offs)
#define TS_BADVADDR(offs)  ((36 * ASM_WORD_BYTES) + offs)
#define TS_STATUS(offs)    ((38 * ASM_WORD_BYTES) + offs)
#define TS_EPC(offs)       ((39 * ASM_WORD_BYTES) + offs)

.macro load_reg reg, ptr, offset=CALL_FRAME_SIZE
	ASM_L	$\reg, TS_REG(\reg, \offset)($\ptr)
.endm

.macro load_reg_safe reg, ptr, offset=CALL_FRAME_SIZE
	.if \reg != \ptr
		load_reg	reg=\reg, ptr=\ptr, offset=\offset
	.endif
.endm

.macro save_reg reg, ptr, offset=CALL_FRAME_SIZE
	ASM_S	$\reg, TS_REG(\reg, \offset)($\ptr)
.endm

.macro save_reg_safe reg, ptr, offset=CALL_FRAME_SIZE
	.if \reg != \ptr
		save_reg	reg=\reg, ptr=\ptr, offset=\offset
	.endif
.endm

/**
 * Restore CP0 Status keeping IM0-IM7 + Impl bits (8-18)
 * \param ts_base  Base pointer register to a trap-state (with offset
 *                 `ts_offset`)
 * \param ts_offset  Offset of the trap-state structure relative to
 *                   `ts_base`
 * uses k0, k1, and s0 as scratch
 */
.macro restore_cp0_status ts_base, ts_offset
	mfc0	k0, CP0_STATUS
	ext	k1, k0, 6, 26
	ASM_L	k0, TS_STATUS(\ts_offset)(\ts_base)
	ins	k0, k1, 6, 26
#ifdef CONFIG_CPU_VIRT
	ASM_ALTERNATIVE_ORIG_START
	mtc0	k0, CP0_STATUS
	b	7216f
	  nop
	ASM_ALTERNATIVE_ORIG_END
	ASM_ALTERNATIVE_ALT_START 1, FEATURE_VZ
	ext	k1, k0, 3, 1
	ins	k0, zero, 3, 1
	beq	k1, zero, 7214f
	  mtc0	k0, CP0_STATUS
	mfc0	k0, CP0_GUESTCTL0
	ins	k0, k1, 31, 1
	mtc0	k0, CP0_GUESTCTL0
#ifdef CONFIG_PF_SEAD3
/* The CPU implementation on the FPGA SEAD3 board has a
   buggy guest timer and does not signal timer interrupts
   correctly to the guest. Work around this by manually
   injecting the timer interrupt via VIP instead.
   Note that this only works as long as Fiasco is uses a
   periodic timer.
*/
	.set push
	.set virt
	.set noreorder
	mfgc0	k1, CP0_CAUSE
	mfc0	k0, CP0_GUESTCTL2
	srl	k1, k1, 30
	ins	k0, k1, 15, 1
	mtc0	k0, CP0_GUESTCTL2
	.set pop
#endif
7214:
	ASM_ALTERNATIVE_ALT_END 1
7216:
#else
	mtc0	k0, CP0_STATUS
#endif
.endm

/**
 * Restore all registers and do an ERET
 * \param ptr  register with the pointer to the trap-state.
 *             The register must be either `sp` or `k1` and may point
 *             `ts_offset` bytes before the actual trap-state.
 * \param ts_offset  number of bytes that must be added to `ptr` to
 *                   address the trap-state structure.
 */
.macro restore_all_and_eret ptr=29, ts_offset=CALL_FRAME_SIZE, adjust_pc=0
	ASM_L	t0, TS_EPC(\ts_offset)($\ptr)
	.if \adjust_pc != 0
		ASM_ADDIU	t0, t0, \adjust_pc
	.endif
#ifndef CONFIG_CPU_MIPSR6
	ASM_L	t1, TS_HI(\ts_offset)($\ptr)
	ASM_L	t2, TS_LO(\ts_offset)($\ptr)
	mthi	t1
	mtlo	t2
#endif
	ASM_MTC0	t0, CP0_EPC
	restore_cp0_status	$\ptr, \ts_offset
	# we also resotore k0 (26) and k1 (27) here even if not saved without
	# VZ eanbled, this might cost some cycles but prevents leakage of
	# kernel values to the user
	.irp reg, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 \
	          16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,   \
	          28, 29, 30, 31
		load_reg_safe	\reg, \ptr, \ts_offset
	.endr
	ehb
	/* do not restore k0 and k1 from TS */
	load_reg	\ptr, \ptr, \ts_offset
	eret
.endm

/**
 * Restore saved (s0-s8, gp, sp, ra) registers and do an ERET
 * \param ptr  register with the pointer to the trap-state.
 *             The register must be either `sp` or any temp register
 *             and may point `ts_offset` bytes before the actual trap-state.
 * \param ts_offset  number of bytes that must be added to `ptr` to
 *                   address the trap-state structure.
 */
.macro restore_saved_and_eret ptr=29, ts_offset=CALL_FRAME_SIZE, adjust_pc=0
	ASM_L	k0, TS_EPC(\ts_offset)($\ptr)
	.if \adjust_pc != 0
		ASM_ADDIU	k0, k0, \adjust_pc
	.endif
	ASM_MTC0	k0, CP0_EPC
	restore_cp0_status	$\ptr, \ts_offset
	.irp reg, 16, 17, 18, 19, 20, 21, 22, 23, 26, 27, 28, 29, 30, 31
		load_reg_safe	\reg, \ptr, \ts_offset
	.endr
	ehb
	/* do not restore \ptr if it is not a callee saved register */
	.if (\ptr >= 16 && \ptr <=23) || \ptr >= 28
		load_reg	\ptr, \ptr, \ts_offset
	.endif
	eret
.endm

.macro save_all ptr=29, ts_offset=CALL_FRAME_SIZE
	# also save zero register as we use the r[0] slot for flaging pre-eret
	# work if it is not 'zero'
	# the at (1) register is already saved
	.irp reg, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, \
	          17, 18, 19, 20, 21, 22, 23, 24, 25
		save_reg_safe	\reg, \ptr, \ts_offset
	.endr
#ifdef CONFIG_CPU_VIRT
	ASM_ALTERNATIVE_ORIG_START
	b	5886f
	  nop
	ASM_ALTERNATIVE_ORIG_END
	ASM_ALTERNATIVE_ALT_START 1, FEATURE_VZ
	ASM_MFC0	t0, CP0_KSCRATCH1
	ASM_MFC0	t1, CP0_KSCRATCH2
	ASM_S	t0, TS_REG(26, \ts_offset)($\ptr)
	ASM_S	t1, TS_REG(27, \ts_offset)($\ptr)
	ASM_ALTERNATIVE_ALT_END 1
5886:
#endif
#ifndef CONFIG_CPU_MIPSR6
	mfhi	t0
	mflo	t1
	ASM_S	t0, TS_HI(\ts_offset)($\ptr)
	ASM_S	t1, TS_LO(\ts_offset)($\ptr)
#endif
	.irp reg, 28, 29, 30, 31
		save_reg_safe	\reg, \ptr, \ts_offset
	.endr
.endm

.macro do_eret restore, save_rest=nix, adjust_pc=0
	.set push
	.set noreorder
	.set noat

	ASM_L	k0, TS_REG(0, CALL_FRAME_SIZE)(sp)
	bnez	k0, 6931f
	  nop
	\restore	29, CALL_FRAME_SIZE, adjust_pc=\adjust_pc
6931:
	.if \save_rest != nix
		\save_rest	29, CALL_FRAME_SIZE
	.endif
	jr	k0
	  ASM_S	zero, TS_REG(0, CALL_FRAME_SIZE)(sp) /* delay slot */

	.set pop
.endm


.section .text
.set noreorder
.set noat

LEAF(ret_from_user_invoke)
	do_eret restore_all_and_eret
END(ret_from_user_invoke)

.section ".text.fast_ret_from_irq", "ax"
LEAF(fast_ret_from_irq)
	restore_saved_and_eret	29, ASM_WORD_BYTES, adjust_pc=4 /* sp offset 4 */
END(fast_ret_from_irq)

.section ".text.vcpu_resume", "ax"
LEAF(vcpu_resume)
	restore_all_and_eret	4, 0 /* a0 offset 0 as pointer to trap-state */
END(vcpu_resume)

.section ".text.leave_by_trigger_exception", "ax"
LEAF(leave_by_trigger_exception)
	.set push
	.set at
	lw	t0, TS_CAUSE(CALL_FRAME_SIZE)(sp)
	and	a0, t0, ~0x3ff
	or	a0, a0, 0x101  /* flag SW exception */
	sw	a0, TS_CAUSE(CALL_FRAME_SIZE)(sp)
	/* clear continuation */
	ASM_S	zero, TS_REG(0, CALL_FRAME_SIZE)(sp)
	/* ra is ret_from_exception already, do a tail call */
	j	thread_handle_trap
	  ASM_ADDIU	a1, sp, CALL_FRAME_SIZE
	.set pop
END(leave_by_trigger_exception)

.macro ENTRY name
	.section .text.exc. ## name ## _entry, "ax"
	.set noreorder
	.set noat
	.type \name\()_entry, function
	.ent \name\()_entry
\name\()_entry:
.endm

.macro END_ENTRY name
	.end \name\()_entry
.endm

ENTRY irq
	# NOTE: we could save the callee-saved registers only and
	#       save the rest only in case of pre-eret work
	save_all	29 # save all registers use sp ($29) as pointer
	mfc0	k0, CP0_STATUS
	andi	k1, k1, 0xff00 # keep IP bits
	and	k1, k1, k0
	beqz	k1, 1f  # skip spourius IRQ
	  clz	k1, k1
	jal	handle_irq
	  xori	a0, k1, 0x17 # 16..23 => 7..0 (a0, as argument)
1:
	do_eret	restore_all_and_eret
END_ENTRY irq

ENTRY tlb_fault
	save_all	29
	ASM_MFC0	a2, CP0_BADVADDR
	move	a0, k1                  # cause as arg1
	ASM_ADDIU	a1, sp, CALL_FRAME_SIZE # ts as arg2

#ifdef CONFIG_CPU_VIRT
  ASM_ALTERNATIVE_ORIG_START
	jal	thread_handle_tlb_fault
	  ASM_S	a2, TS_BADVADDR(CALL_FRAME_SIZE)(sp)
  ASM_ALTERNATIVE_ORIG_END
  ASM_ALTERNATIVE_ALT_START 1, FEATURE_VZ
	jal	thread_handle_tlb_fault_vz
	  ASM_S	a2, TS_BADVADDR(CALL_FRAME_SIZE)(sp)
  ASM_ALTERNATIVE_ALT_END 1
#else
	jal	thread_handle_tlb_fault
	  ASM_S	a2, TS_BADVADDR(CALL_FRAME_SIZE)(sp)
#endif

	do_eret	restore_all_and_eret
END_ENTRY tlb_fault

ENTRY slowtrap
	# NOTE: we could save the callee-saved registers only and
	#       save the rest only in case of pre-eret work
	save_all	29 # save all registers use sp ($29) as pointer
	ASM_MFC0	a2, CP0_BADVADDR
	move	a0, k1                  # cause as arg1
	ASM_S	a2, TS_BADVADDR(CALL_FRAME_SIZE)(sp)
	jal	thread_handle_trap
	  ASM_ADDIU	a1, sp, CALL_FRAME_SIZE # ts as arg2
	do_eret	restore_all_and_eret
END_ENTRY slowtrap

ENTRY copu
	# NOTE: we could save the callee-saved registers only and
	#       save the rest only in case of pre-eret work
	save_all	29 # save all registers use sp ($29) as pointer
	ASM_ADDIU	a1, sp, CALL_FRAME_SIZE # ts as arg2
	ext	a2, k1, 28, 2
	ASM_ADDIU	a2, a2, -1
	bnez	a2, 1f
	  move	a0, k1                  # cause as arg1
	jal	handle_fpu_trap
	  nop
2:
	do_eret	restore_all_and_eret
1:
	jal	thread_handle_trap
	  nop
	b	2b
END_ENTRY copu


ENTRY unhandled
	save_all	29
	move	a0, k1
	jal	thread_unhandled_trap
	  ASM_ADDIU	a1, sp, CALL_FRAME_SIZE # ts as arg2
	do_eret	restore_all_and_eret
END_ENTRY unhandled

ENTRY syscall
	.global sys_ipc_call_patch
	save_all	29
sys_ipc_call_patch:
	jal	sys_ipc_wrapper
	  nop
	do_eret	restore_all_and_eret adjust_pc=4
END_ENTRY syscall

ENTRY reserved_insn
	# NOTE. currently does not work for guest reserved insn redirect
	# Handle emulation of rdhwr instruction for TLS (ULR)
	ASM_MFC0	k0, CP0_EPC
	lw	k1, 0(k0) # load instruction
	li	k0,  0x7c00e83b
	li	$at, 0xffe0ffff
	and	$at, $at, k1   # check for rdhwr .., $29
	bne	$at, k0, 1f    # do normal slowtrap entry if not rdhwr .., $29
	  ext	k0, k1, 16, 5  # extract target register number

	move	$at, sp
	ins	$at, zero, 0, THREAD_BLOCK_SHIFT # TCB addr in $at
	sll	k0, k0, 3      # 32 insn bundles (2 insns) one for each target
	ASM_LA	k1, 2f         # register at 2f
	ASM_ADDU	k0, k1, k0     # calculate the address of the target register load
	ASM_L	k1, TS_STATUS(CALL_FRAME_SIZE)(sp)
	mtc0	k1, CP0_STATUS # restore CP0 Status as the entry code modified it
	ASM_MFC0	k1, CP0_EPC    # skip the rdhwr insn
	ASM_ADDIU	k1, k1, 4
	ASM_MTC0	k1, CP0_EPC
	jr	k0             # jump to the register loading code
	  ehb                  # prepare for eret

	# Load the target register with the ULR value
	# we have 32 instruction bundles that load the
	# target register and branch to the exit code
2:
	b	3f
	  nop
	b	4f
	  ASM_L	$at, OFS__THREAD__ULR($at)
	.irp reg, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, \
	          16, 17, 18,  19, 20, 21, 22, 23, 24, 25
		b	3f
		  ASM_L	$\reg, OFS__THREAD__ULR($at)
	.endr
	b	3f
	  nop
	b	3f
	  nop
	b	3f
	  ASM_L	$28, OFS__THREAD__ULR($at)
	b	3f    # ignore store to sp (r29)
	  nop
	b	3f
	  ASM_L	$30, OFS__THREAD__ULR($at)
	nop
	ASM_L	$31, OFS__THREAD__ULR($at)

3:	load_reg	 1, 29
4:	load_reg	29, 29
	/* no need to restore k0, k1 here as this must never happen
	 * from guest mit with VZ */
	eret

1: # do normal slowtrap entry
	save_all	29
	mfc0	a0, CP0_CAUSE
	jal	thread_handle_trap
	  ASM_ADDIU	a1, sp, CALL_FRAME_SIZE # ts as arg2
	do_eret	restore_all_and_eret
END_ENTRY reserved_insn


/* pass JDB trap code to Thread::call_nested_trap_handler */
.macro DEBUGGER_ENTRY Jdb_trap_code
#ifdef CONFIG_JDB
	li	t0, \Jdb_trap_code
	break
#endif
	j	ra
	  nop
.endm

.section ".text.kdebug_entry", "ax"
.set noreorder
LEAF(kern_kdebug_entry)
	DEBUGGER_ENTRY	0x00000000
END(kern_kdebug_entry)

LEAF(kern_kdebug_sequence_entry)
	DEBUGGER_ENTRY	0x00000001
END(kern_kdebug_sequence_entry)

#ifdef CONFIG_MP
LEAF(kern_kdebug_ipi_entry)
	DEBUGGER_ENTRY	0x00000002
END(kern_kdebug_ipi_entry)
#endif

#define ENTRY_ADDR(name) .word name ## _entry
#define ENTRY_INVALID    .word 0xffffffff

.section ".rodata.exc_vect_table", "a"
.align 0x8
.Lexc_vect_table:
	ENTRY_ADDR(irq)
	ENTRY_ADDR(tlb_fault)  # Mod
	ENTRY_ADDR(tlb_fault)  # TLBL
	ENTRY_ADDR(tlb_fault)  # TLBS
	ENTRY_ADDR(slowtrap)   # AdEL
	ENTRY_ADDR(slowtrap)   # AdES
	ENTRY_ADDR(slowtrap)   # IBE
	ENTRY_ADDR(slowtrap)   # DBE
	ENTRY_ADDR(syscall)    # Sys
	ENTRY_ADDR(slowtrap) # Break point
	ENTRY_ADDR(reserved_insn) # reserved instruction
	ENTRY_ADDR(copu)  # CpU
	ENTRY_ADDR(slowtrap) # Overflow
	ENTRY_ADDR(slowtrap) # Trap (conditional break point)
	ENTRY_INVALID # MSAFPE
	ENTRY_INVALID # FPE
	ENTRY_ADDR(unhandled)  # Res
	ENTRY_ADDR(unhandled)  # Res
	ENTRY_INVALID # C2E
	ENTRY_ADDR(tlb_fault)  # TLBRI
	ENTRY_ADDR(tlb_fault)  # TLBXI
	ENTRY_INVALID # MSADis
	ENTRY_INVALID # MDMX
	ENTRY_INVALID # WATCH
	ENTRY_ADDR(unhandled) # MCheck
	ENTRY_INVALID # Thread
	ENTRY_INVALID # DSPDis
	ENTRY_ADDR(slowtrap) # GE
	ENTRY_ADDR(unhandled)  # Res
	ENTRY_ADDR(unhandled)  # Res
	ENTRY_ADDR(unhandled)  # CacheErr
	ENTRY_ADDR(unhandled)  # Res

/**
 * Load Cp0 Status into k0
 * With VZ also put the GuestCtl0.GM bit into bit 3 of the status in k0
 * and clear GuestCtl0.GM
 */
.macro generic_exception_entry
#ifdef CONFIG_CPU_VIRT
	ASM_ALTERNATIVE_ORIG_START
	mfc0	k0, CP0_STATUS
	b	6157f
	  nop
	ASM_ALTERNATIVE_ORIG_END
	ASM_ALTERNATIVE_ALT_START 1, FEATURE_VZ
	# save k0 and k1 and pick the guest mode bit and put it into
	# status KSU supervisor mode bit as we never use supervisor mode
	ASM_MTC0	k0, CP0_KSCRATCH1
	ASM_MTC0	k1, CP0_KSCRATCH2
	mfc0	k0, CP0_GUESTCTL0
	ext	k1, k0, 31, 1
	ins	k0, zero, 31, 1
	mtc0	k0, CP0_GUESTCTL0
	mfc0	k0, CP0_STATUS
	ins	k0, k1, 3, 1
	ASM_ALTERNATIVE_ALT_END 1
#else
	mfc0	k0, CP0_STATUS
#endif
6157:
.endm

.section ".exception.TLB_REFILL", "ax"
.set noreorder
.ent mips_tlb_refill_handler
mips_tlb_refill_handler:
	generic_exception_entry
	ext	k1, k0, 3, 2
	beqz	k1, 1f           # if coming from kernel mode skip loading k-sp
	  move	k1, sp           # capture the current (kernel) sp
	ASM_MFC0	k1, CP0_ERROREPC # load the kernel SP in ErrorEPC
1:
	ASM_ADDIU	k1, k1, -(TS_FRAME + CALL_FRAME_SIZE)
	save_reg	29, 27   # save prev stack pointer at k1
	save_reg	 1, 27   # save at register at k1
	move	sp, k1           # now we are running on the kernel stack
	ASM_S	k0, TS_STATUS(CALL_FRAME_SIZE)(k1)
	ins	k0, zero, 0, 5   # clear IE, EXL, ERL, KSU (kernel mode)
	mtc0	k0, CP0_STATUS
	mfc0	k1, CP0_CAUSE
	ASM_S	k1, TS_CAUSE(CALL_FRAME_SIZE)(sp)
	ASM_MFC0	k0, CP0_EPC
	ASM_S	k0, TS_EPC(CALL_FRAME_SIZE)(sp)
	ori	k1, k1, 1 # flag TLB refill in bit 0 of cause
	                  # Uses reserved Cause bit 0 (see thread-mips.cpp)
	j	tlb_fault_entry
	  ehb
.end mips_tlb_refill_handler

.section ".exception.XTLB_REFILL", "ax"
.set noreorder
.ent mips_xtlb_refill_handler
mips_xtlb_refill_handler:
	generic_exception_entry
	ext	k1, k0, 3, 2
	beqz	k1, 1f           # if coming from kernel mode skip loading k-sp
	  move	k1, sp           # capture the current (kernel) sp
	ASM_MFC0	k1, CP0_ERROREPC # load the kernel SP in ErrorEPC
1:
	ASM_ADDIU	k1, k1, -(TS_FRAME + CALL_FRAME_SIZE)
	save_reg	29, 27   # save prev stack pointer at k1
	save_reg	 1, 27   # save at register at k1
	move	sp, k1           # now we are running on the kernel stack
	ASM_S	k0, TS_STATUS(CALL_FRAME_SIZE)(k1)
	ins	k0, zero, 0, 5   # clear IE, EXL, ERL, KSU (kernel mode)
	mtc0	k0, CP0_STATUS
	mfc0	k1, CP0_CAUSE
	ASM_S	k1, TS_CAUSE(CALL_FRAME_SIZE)(sp)
	ASM_MFC0	k0, CP0_EPC
	ASM_S	k0, TS_EPC(CALL_FRAME_SIZE)(sp)
	ori	k1, k1, 1 # flag TLB refill in bit 0 of cause
	                  # Uses reserved Cause bit 0 (see thread-mips.cpp)
	j	tlb_fault_entry
	  ehb
.end mips_xtlb_refill_handler

.section ".exception.GEN", "ax"
.set noreorder
.ent mips_generic_exception_handler
.global mips_generic_exception_handler
mips_generic_exception_handler:
	generic_exception_entry
	ext	k1, k0, 3, 2
	beqz	k1, 1f           # if coming from kernel mode skip loading k-sp
	  move	k1, sp           # capture the current (kernel) sp
	ASM_MFC0	k1, CP0_ERROREPC # load the kernel SP in ErrorEPC
1:
	ASM_ADDIU	k1, k1, -(TS_FRAME + CALL_FRAME_SIZE)
	save_reg	29, 27 # save prev stack pointer at k1
	save_reg	 1, 27 # save at register at k1
	move	sp, k1         # now we are running on the kernel stack
	ASM_S	k0, TS_STATUS(CALL_FRAME_SIZE)(k1)
	ins	k0, zero, 0, 5   # clear IE, EXL, ERL, KSU (kernel mode)
	mtc0	k0, CP0_STATUS
	mfc0	k1, CP0_CAUSE
	ASM_S	k1, TS_CAUSE(CALL_FRAME_SIZE)(sp)
	ASM_MFC0	k0, CP0_EPC
	ASM_S	k0, TS_EPC(CALL_FRAME_SIZE)(sp)
	andi	k0, k1, 0x7c     # capture the ExcCode
	ASM_LA	$at, .Lexc_vect_table
	ASM_ADDU	k0, k0, $at
	lw	$at, 0(k0)
	j	$at
	  ehb
.end mips_generic_exception_handler
.global mips_generic_exception_handler_end
mips_generic_exception_handler_end:

.section .text

