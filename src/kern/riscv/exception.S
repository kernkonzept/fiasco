/* -*- asm -*- */

#include "asm_riscv.h"
#include "globalconfig.h"

#define SR_SIE  0x00000002 /* Supervisor Interrupt Enable */
#define SR_SPIE 0x00000020 /* Previous Supervisor Interrupt Enable */
#define SR_SPP  0x00000100 /* Previously Supervisor */

#define FS_MASK 0x00006000 /* FS */

#define hstatus 0x600
#define HSTATUS_SPV  0x00000080
#define HSTATUS_SPVP 0x00000100

#define TS_ERETW  (0 * SZREG)
#define TS_ra     (1 * SZREG)
#define TS_sp     (2 * SZREG)
#define TS_gp     (3 * SZREG)
#define TS_tp     (4 * SZREG)
#define TS_t0     (5 * SZREG)
#define TS_t1     (6 * SZREG)
#define TS_t2     (7 * SZREG)
#define TS_s0     (8 * SZREG)
#define TS_s1     (9 * SZREG)
#define TS_a0     (10 * SZREG)
#define TS_a1     (11 * SZREG)
#define TS_a2     (12 * SZREG)
#define TS_a3     (13 * SZREG)
#define TS_a4     (14 * SZREG)
#define TS_a5     (15 * SZREG)
#define TS_a6     (16 * SZREG)
#define TS_a7     (17 * SZREG)
#define TS_s2     (18 * SZREG)
#define TS_s3     (19 * SZREG)
#define TS_s4     (20 * SZREG)
#define TS_s5     (21 * SZREG)
#define TS_s6     (22 * SZREG)
#define TS_s7     (23 * SZREG)
#define TS_s8     (24 * SZREG)
#define TS_s9     (25 * SZREG)
#define TS_s10    (26 * SZREG)
#define TS_s11    (27 * SZREG)
#define TS_t3     (28 * SZREG)
#define TS_t4     (29 * SZREG)
#define TS_t5     (30 * SZREG)
#define TS_t6     (31 * SZREG)
#define TS_EPC    (32 * SZREG)
#define TS_STATUS (33 * SZREG)
#define TS_CAUSE  (34 * SZREG)
#define TS_TVAL   (35 * SZREG)
/* Only used if virtualization is enabled. */
#define TS_HSTATUS (36 * SZREG)

/* Frame size, must be a multiple of 16 byte (stack alignment is 16 byte) */
#define TS_FRAME_PAD (16 - (1 * SZREG))
#define TS_FRAME     ((37 * SZREG) + TS_FRAME_PAD)

/* Hart context */
#define HC_KSP    (0 * SZREG)
#define HC_TSP    (1 * SZREG)

.macro save_reg reg reg_offset from=none
    .if \from == none
        REG_S \reg, \reg_offset(sp)
    .else
        REG_S \from, \reg_offset(sp)
    .endif
.endm

.macro restore_reg reg reg_offset base=sp force=false
    /* Only restore if target register != base register. */
    .if \base != \reg || \force == true
        REG_L \reg, \reg_offset(\base)
    .endif
.endm

.macro clear_bits rd rt bits
    li \rt, \bits
    not \rt, \rt
    and \rd, \rd, \rt
.endm

.macro set_bits rd rt bits=reg
    .if \bits != reg
        li \rt, \bits
    .endif
    or \rd, \rd, \rt
.endm

.macro mask_bits rd rt bits=reg
    .if \bits != reg
        li \rt, \bits
    .endif
    and \rd, \rd, \rt
.endm

.macro save_all
    /* If coming from userspace, sscratch contains the kernel thread pointer,
       which points to the hart context.
       If coming from kernel, sscratch contains zero. */
    csrrw tp, sscratch, tp
    bnez tp, _load_kernel_sp\@

    /* We came from kernel, restore kernel thread pointer. */
    csrr tp, sscratch

    /* Save kernel stack pointer */
    REG_S sp, HC_TSP(tp)
    j _save_trap_state\@

_load_kernel_sp\@:
    /* We came from userspace, save user stack pointer */
    REG_S sp, HC_TSP(tp)

    /* Load kernel stack pointer */
    REG_L sp, HC_KSP(tp)

_save_trap_state\@:
    /* Setup entry frame */
    add sp, sp, -TS_FRAME

    REG_S zero, TS_ERETW(sp)
    save_reg ra, TS_ra
    save_reg gp, TS_gp
    save_reg t0, TS_t0
    save_reg t1, TS_t1
    save_reg t2, TS_t2
    save_reg s0, TS_s0
    save_reg s1, TS_s1
    save_reg a0, TS_a0
    save_reg a1, TS_a1
    save_reg a2, TS_a2
    save_reg a3, TS_a3
    save_reg a4, TS_a4
    save_reg a5, TS_a5
    save_reg a6, TS_a6
    save_reg a7, TS_a7
    save_reg s2, TS_s2
    save_reg s3, TS_s3
    save_reg s4, TS_s4
    save_reg s5, TS_s5
    save_reg s6, TS_s6
    save_reg s7, TS_s7
    save_reg s8, TS_s8
    save_reg s9, TS_s9
    save_reg s10, TS_s10
    save_reg s11, TS_s11
    save_reg t3, TS_t3
    save_reg t4, TS_t4
    save_reg t5, TS_t5
    save_reg t6, TS_t6

    /* Save trap sp */
    REG_L t0, HC_TSP(tp)
    save_reg sp, TS_sp, t0

    /* Save trap tp */
    csrr t0, sscratch
    save_reg tp, TS_tp, t0

    /* Save sepc */
    csrr t0, sepc
    REG_S t0, TS_EPC(sp)

    /* Save sstatus */
    csrr t0, sstatus
    REG_S t0, TS_STATUS(sp)

    /* Save scause */
    csrr t0, scause
    REG_S t0, TS_CAUSE(sp)

    /* Save stval */
    csrr t0, stval
    REG_S t0, TS_TVAL(sp)

#ifdef CONFIG_CPU_VIRT
    /* Save hstatus */
    csrr t0, hstatus
    REG_S t0, TS_HSTATUS(sp)
#endif

    /* Set sscratch to zero, so that a nested trap knows it came from kernel. */
    csrw sscratch, x0
.endm

.macro restore_sret base=sp
    /* Set sret return address */
    REG_L t1, TS_EPC(\base)
    csrw sepc, t1

    /* Preserve FPU status */
    csrr t1, sstatus
    mask_bits t1, t2, FS_MASK

    /* Restore sstatus */
    REG_L t0, TS_STATUS(\base)
    clear_bits t0, t2, FS_MASK
    set_bits t0, t1

#ifdef CONFIG_CPU_VIRT
    /* Restore hstatus */
    REG_L t1, TS_HSTATUS(\base)
    csrw hstatus, t1

    /* Returning to virt mode? */
    andi t2, t1, HSTATUS_SPV
    beqz t2, _ret_non_virt_mode\@

    /* Set SR_SPP to HSTATUS_SPVP */
    clear_bits t0, t2, SR_SPP
    andi t2, t1, HSTATUS_SPVP
    set_bits t0, t2 /* SR_SPP and HSTATUS_SPVP flags have the same offset */
    j _save_kernel_sptp\@
#endif

_ret_non_virt_mode\@:
    /* Returning to userspace or kernel? */
    andi t1, t0, SR_SPP
    bnez t1, _skip_save_kernel_sptp\@

_save_kernel_sptp\@: /* Returning to userspace or virt mode */
    /* Save kernel stack pointer in hart context. */
    add t1, sp, TS_FRAME
    REG_S t1, HC_KSP(tp)

    /* Save kernel thread pointer in sscratch. */
    csrw sscratch, tp

    /* Restore tp register only when switching to userspace. In the kernel,
       the tp register points to a per CPU hard context, i.e. contains a fixed
       value for each CPU. Therefore, for kernel trap states the tp register
       must not be restored, otherwise resuming a thread that was interrupted
       on CPU A on a CPU B would overwrite the tp on CPU B to point to CPU A's
       hart context. */
    restore_reg tp, TS_tp, \base

_skip_save_kernel_sptp\@:
    /* Restore sstatus */
    csrw sstatus, t0
.endm

.macro restore_all base=sp base_offset=TS_sp
    restore_sret \base

    restore_reg ra, TS_ra, \base
    restore_reg sp, TS_sp, \base
    restore_reg gp, TS_gp, \base
    restore_reg t0, TS_t0, \base
    restore_reg t1, TS_t1, \base
    restore_reg t2, TS_t2, \base
    restore_reg s0, TS_s0, \base
    restore_reg s1, TS_s1, \base
    restore_reg a0, TS_a0, \base
    restore_reg a1, TS_a1, \base
    restore_reg a2, TS_a2, \base
    restore_reg a3, TS_a3, \base
    restore_reg a4, TS_a4, \base
    restore_reg a5, TS_a5, \base
    restore_reg a6, TS_a6, \base
    restore_reg a7, TS_a7, \base
    restore_reg s2, TS_s2, \base
    restore_reg s3, TS_s3, \base
    restore_reg s4, TS_s4, \base
    restore_reg s5, TS_s5, \base
    restore_reg s6, TS_s6, \base
    restore_reg s7, TS_s7, \base
    restore_reg s8, TS_s8, \base
    restore_reg s9, TS_s9, \base
    restore_reg s10, TS_s10, \base
    restore_reg s11, TS_s11, \base
    restore_reg t3, TS_t3, \base
    restore_reg t4, TS_t4, \base
    restore_reg t5, TS_t5, \base
    restore_reg t6, TS_t6, \base

    /* Restore base register last */
    restore_reg \base, \base_offset, \base, true
.endm


.macro handle_eret_work ret
    REG_L t0, TS_ERETW(sp)
    beqz t0, _no_continuation_pending\@

    la ra, \ret
    /* Reset continuation */
    REG_S zero, TS_ERETW(sp)

    /* Pass return frame as argument */
    mv a0, sp
    jr t0

_no_continuation_pending\@:
.endm


ENTRY(handle_trap)

    save_all

.option push
.option norelax
    /* Load the global pointer. */
    la gp, __global_pointer$
.option pop

    csrr a0, scause
    csrr a1, stval
    mv a2, sp
    la ra, return_from_exception
    j thread_handle_trap
END(handle_trap)


/**
 * Restore vCPU state and reset kernel stack pointer to top of stack.
 *
 * \pre Interrupts must be disabled.
 */
ENTRY(vcpu_resume)
    /* Reset kernel stack pointer to return frame */
    mv sp, a1

    /* Trap_state is passed as first argument */
    restore_all a0, TS_a0

    sret
END(vcpu_resume)


ENTRY(leave_by_trigger_exception)
    /* Set cause to Ec_l4_exregs_exception */
    li t0, 0x19
    REG_S t0, TS_CAUSE(sp)

    /* Pass Trap_state as argument */
    mv a0, sp
    j thread_handle_slow_trap
END(leave_by_trigger_exception)


ENTRY(leave_and_kill_myself)
    /* Make space for a dummy Return_frame accessible by the callee */
    add sp, sp, -TS_FRAME
    j thread_do_leave_and_kill_myself
END(leave_and_kill_myself)


/**
 * Restores minimal execution environment from return frame.
 *
 * \pre Interrupts must be disabled.
 */
ENTRY(fast_sret)
    restore_sret sp
    restore_reg gp, TS_gp, sp
    restore_reg sp, TS_sp, sp, true

    /* Clear all registers before returning to vCPU entry handler. */
    /* a0 == vCPU user pointer */
    li ra, 0
    li t0, 0
    li t1, 0
    li t2, 0
    li s0, 0
    li s1, 0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    li a5, 0
    li a6, 0
    li a7, 0
    li s2, 0
    li s3, 0
    li s4, 0
    li s5, 0
    li s6, 0
    li s7, 0
    li s8, 0
    li s9, 0
    li s10, 0
    li s11, 0
    li t3, 0
    li t4, 0
    li t5, 0
    li t6, 0

    sret
END(fast_sret)


/**
 * First handle eret work if any, then restore state from Return_frame at sp.
 *
 * \pre Interrupts must be disabled.
 */
ENTRY(return_from_exception)
ENTRY(return_from_user_invoke)
    /* Check for pending continuation */
    handle_eret_work return_from_exception

    restore_all

    sret
END(return_from_exception)
END(return_from_user_invoke)


/**
 * Restore state from Entry_frame at sp without checking for eret work.
 *
 * \pre Interrupts must be disabled.
 */
ENTRY(fast_ret_from_irq)
    restore_all

    sret
END(fast_ret_from_irq)
