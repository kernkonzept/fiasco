/* -*- asm -*- */

#include "globalconfig.h"
#include "config_tcbsize.h"
#include "tcboffset.h"

#ifdef CONFIG_CPU_VIRT
#define EL(x) x##_el2
#define PRENR   S3_4_c6_c1_1
#define PRBAR1  S3_4_c6_c8_4
#define PRBAR2  S3_4_c6_c9_0
#define PRLAR2  S3_4_c6_c9_1
#else
#define EL(x) x##_el1
#define PRENR   S3_0_c6_c1_1
#define PRBAR1  S3_0_c6_c8_4
#define PRBAR2  S3_0_c6_c9_0
#define PRLAR2  S3_0_c6_c9_1
#endif

#define EF_ERETW  ( 0 * 8)
#define EF_KSP    (32 * 8)
#define EF_ESR    (33 * 8)
#define EF_PFA    (34 * 8)
#define EF_SP     (35 * 8)
#define EF_PC     (36 * 8)
#define EF_PSTATE (37 * 8)
#define EF_SIZE   (38 * 8)

.macro CONTEXT_OF reg, ptr
	mov	\reg, \ptr
	and	\reg, \reg, #~(THREAD_BLOCK_SIZE-1)
.endm

.macro save_gprs no_7_to_12=0
	stp	xzr, x0,   [sp]
	stp	x1, x2,    [sp,  #16]
	stp	x3, x4,    [sp,  #32]
	stp	x5, x6,    [sp,  #48]
	.if \no_7_to_12 == 0
		stp	x7, x8,    [sp,  #64]
		stp	x9, x10,   [sp,  #80]
		stp	x11, x12,  [sp,  #96]
	.endif
	stp	x13, x14,  [sp, #112]
	stp	x15, x16,  [sp, #128]
	stp	x17, x18,  [sp, #144]
	stp	x19, x20,  [sp, #160]
	stp	x21, x22,  [sp, #176]
	stp	x23, x24,  [sp, #192]
	stp	x25, x26,  [sp, #208]
	stp	x27, x28,  [sp, #224]
	stp	x29, x30,  [sp, #240]
.endm

.macro restore_gprs freg=sp
	.ifnes "\freg", "x0"
		ldr	x0,        [\freg,   #8]
	.endif
	ldp	x1, x2,    [\freg,  #16]
	ldp	x3, x4,    [\freg,  #32]
	ldp	x5, x6,    [\freg,  #48]
	ldp	x7, x8,    [\freg,  #64]
	ldp	x9, x10,   [\freg,  #80]
	ldp	x11, x12,  [\freg,  #96]
	ldp	x13, x14,  [\freg, #112]
	ldp	x15, x16,  [\freg, #128]
	ldp	x17, x18,  [\freg, #144]
	ldp	x19, x20,  [\freg, #160]
	ldp	x21, x22,  [\freg, #176]
	ldp	x23, x24,  [\freg, #192]
	ldp	x25, x26,  [\freg, #208]
	ldp	x27, x28,  [\freg, #224]
	ldp	x29, x30,  [\freg, #240]
.endm

.macro load_eret_state freg=sp
	ldp	x3, x4, [\freg, #EF_PC]
	msr	EL(ELR), x3
	msr	EL(SPSR), x4
.endm


.section	.text.excp, "xa"

.type   leave_and_kill_myself, function
.global leave_and_kill_myself
leave_and_kill_myself:
        // make space for a dummy Return_frame accessible by the callee
        sub     sp, sp, #EF_SIZE
        bl      thread_do_leave_and_kill_myself

.type	leave_by_trigger_exception, function
.global	leave_by_trigger_exception
leave_by_trigger_exception:
	mov	x1, #(0x3e << 26)
	str	x1,  [x0, #EF_ESR]
	b	slowtrap_entry

.type	fast_ret_from_irq, function
.global	fast_ret_from_irq
.type	__return_from_user_invoke, function
.global	__return_from_user_invoke
.type	__iret, function
.global	__iret
__iret:
#ifdef CONFIG_MPU
	bl	__mpu_kernel_leave_nocheck
#endif
	load_eret_state sp
	// Clear all registers before returning to vCPU entry handler.
	// x0 == vCPU user pointer
	mov	x1, #0
	mov	x2, #0
	mov	x3, #0
	mov	x4, #0
	mov	x5, #0
	mov	x6, #0
	mov	x7, #0
	mov	x8, #0
	mov	x9, #0
	mov	x10, #0
	mov	x11, #0
	mov	x12, #0
	mov	x13, #0
	mov	x14, #0
	mov	x15, #0
	mov	x16, #0
	mov	x17, #0
	mov	x18, #0
	mov	x19, #0
	mov	x20, #0
	mov	x21, #0
	mov	x22, #0
	mov	x23, #0
	mov	x24, #0
	mov	x25, #0
	mov	x26, #0
	mov	x27, #0
	mov	x28, #0
	mov	x29, #0
	mov	x30, #0
	add	sp, sp, #EF_SIZE
	eret

__return_from_user_invoke:
// In case that a continuation is invoked before exit, make sure that
// the handler returns to the normal exit ret_from_exception exit path and
// finds the kernel stack address at the expected place.
	adr x30, .Lret_from_exception
	str x0, [sp, #EF_KSP]
	b 6f

.Lret_from_exception:
	ldr	x0, [sp, #EF_KSP]
6:
	ldr	x1, [x0, #EF_ERETW]
	cbnz	x1, 1f // handle continuations
	mov	sp, x0
fast_ret_from_irq:
	mrs x5, daif
	cmp x5, #0x3c0
	b.eq 99f
	brk 0x400
99:
#ifdef CONFIG_MPU
	bl	__mpu_kernel_leave
#endif
	load_eret_state sp
	restore_gprs
	add	sp, sp, #EF_SIZE
	eret

1:	str	xzr, [x0, #EF_ERETW] // reset continuation
	br	x1                   // handle continuation in x1

#ifdef CONFIG_MPU
.if OFS__THREAD__MPU_PRBAR2+8 != OFS__THREAD__MPU_PRLAR2
.error "_mpu_prbar2 and _mpu_prlar2 must be adjacent"
.endif

/**
 * Reconfigure MPU for kernel space.
 *
 * Clobbers registers x8-x12. Note that the function returns to x7 instead of
 * lr because the latter was not saved yet!
 */
__mpu_kernel_enter:
	CONTEXT_OF x8, sp

	// restore regular heap region
	ldp	x10, x11, [x8, #OFS__THREAD__MPU_PRBAR2]
	msr	PRBAR2, x10		// restore heap region
	msr	PRLAR2, x11		// ...
	mrs	x12, PRBAR1		// read KIP BAR
	ldr	w9, [x8, #OFS__THREAD__KU_MEM_REGIONS]
	mrs	x8, PRENR
	and	x12, x12, #~(0x3 << 2)	// AP[2:1] = b00 -> RW@EL1
	bic	x9, x8, x9		// disable active ku_mem regions
	msr	PRBAR1, x12		// make KIP writable
	msr	PRENR, x9		// disable all ku_mem user regions

	// Technically we should execute an ISB to make sure the MPU updates
	// have settled. Practically, the relevant CPUs are in-order
	// architectures and do not need this. In the worst case a level 0
	// fault happens which is gracefully handled by
	// Thread::arm_kernel_sync_entry().
	br	x7

/**
 * Reconfigure MPU for user space.
 *
 * Clobbers registers x8-x13. Must spares x0 because it's relevant for the vCPU
 * kernel mode exit path.
 */
__mpu_kernel_leave:
	ldr	x8, [sp, #EF_PSTATE]
	tst	x8, #(1 << 4)		// return to AArch64?
	ubfx	x9, x8, #0, #4		// extract mode bits

	// Check if we stay in kernel mode. The final condition code is "HI".
	// If we go to AArch32 (test for bit 4 above) we leave the kernel for
	// sure and set the condition to "zero == not higer". Otherwise compare
	// with our exeption level.
#ifdef CONFIG_CPU_VIRT
	ccmp	x9, #7, #4, EQ	// to user/guest mode (EL1/0) if AArch64?
#else
	ccmp	x9, #0, #4, EQ	// to user mode (EL0) if AArch64?
#endif
	b.hi	1f

__mpu_kernel_leave_nocheck:
	CONTEXT_OF x8, sp
	ldr	w9, [x8, #OFS__THREAD__KU_MEM_REGIONS]
	mrs	x10, PRBAR2
	mrs	x11, PRLAR2
	mrs	x13, PRBAR1

	// constrain heap region to stack
	orr	x12, x8, #((THREAD_BLOCK_SIZE-1) & ~0x3f)
	and	x10, x10, #0x3f
	and	x11, x11, #0x3f
	orr	x10, x10, x8
	orr	x11, x11, x12
	msr	PRBAR2, x10
	msr	PRLAR2, x11

	// make KIP read only
	orr	x13, x13, #(3 << 2)	// AP[2:1] = b11 -> RO@EL2/1/0
	msr	PRBAR1, x13

	// enable ku_mem user regions
	mrs	x10, PRENR
	orr	x9, x10, x9
	msr	PRENR, x9

	// The final eret will implicitly synchronize the MPU updates above.
1:	br	lr
#endif

.type	vcpu_resume, function
.global	vcpu_resume
vcpu_resume:
#ifdef CONFIG_MPU
	bl	__mpu_kernel_leave_nocheck
#endif
	add	sp, x1, #EF_SIZE
	ldr	x3, [x0, #EF_SP]
	msr	SP_EL0, x3
	load_eret_state x0
	restore_gprs x0
	ldr	x0, [x0, #8]
	eret


.global kern_kdebug_ipi_entry
kern_kdebug_ipi_entry:
	brk #3
	ret

.align 3
.global	sys_call_table
sys_call_table:
	.8byte sys_ipc_wrapper

.Lgeneric_entry:
	save_gprs 1
	mrs	x3, EL(ELR)
	mrs	x4, EL(SPSR)
	stp	x3, x4, [sp, #EF_PC]
	mov	x0, sp
	str	x0, [sp, #EF_KSP]
	adr	x30, .Lret_from_exception
	br	x7

.macro ENTRY from_lower func
	sub	sp, sp, #EF_SIZE
	stp	x7, x8,    [sp,  #64]
	stp	x9, x10,   [sp,  #80]
	stp	x11, x12,  [sp,  #96]
#ifdef CONFIG_MPU
	.if \from_lower == 1
	adr	x7, 1f
	b __mpu_kernel_enter
	.endif
#endif
1:	adr	x7, \func
	b .Lgeneric_entry
.endm

// Attention: this macro is currently not usable for entries which come from a
// lower exception level. That would require some MPU handling like in ENTRY.
.macro ENTRY_ESR func
	sub	sp, sp, #EF_SIZE
	stp	x7, x8,    [sp,  #64]
	stp	x9, x10,   [sp,  #80]
	stp	x11, x12,  [sp,  #96]
	mrs	x8, EL(ESR)
	str	x8, [sp, #EF_ESR]
	adr	x7, \func
	b .Lgeneric_entry
.endm

.Lirq_entry:
	sub	sp, sp, #EF_SIZE
	save_gprs
#ifdef CONFIG_MPU
	adr	x7, 1f
	b __mpu_kernel_enter
#endif
1:	mrs	x3, EL(ELR)
	mrs	x4, EL(SPSR)
	stp	x3, x4, [sp, #EF_PC]
	mov	x0, sp
	str	x0, [sp, #EF_KSP]
	adr	x30, .Lret_from_exception

	.global __irq_handler_b_irq
__irq_handler_b_irq:
	b	irq_handler


.section	.text.ivt,"xa"
.align 11
.global exception_vector
exception_vector:
	# from kernel with SP_EL0
	.align 7 /* 0 -- Sync */
	ENTRY 0 arm_kernel_sync_entry

	.align 7 /* 0x80 -- IRQ */
	ENTRY_ESR call_nested_trap_handler

	.align 7 /* 0x100 -- FIQ */
	ENTRY_ESR call_nested_trap_handler

	.align 7 /* 0x180 -- SError */
	ENTRY 0 arm_serror_handler

	# from kernel with SP_ELx
	.align 7 /* 0x200 -- Sync */
	ENTRY 0 arm_kernel_sync_entry

	.align 7 /* 0x280 -- IRQ */
	b	.Lirq_entry

	.align 7 /* 0x300 -- FIQ */
	b	.Lirq_entry

	.align 7 /* 0x380 -- SError */
	ENTRY 0 arm_serror_handler

	# from aarch64 user
	.align 7 /* 0x400 -- Sync */
	ENTRY 1 arm_esr_entry

	.align 7  /* 0x480 -- IRQ */
	b	.Lirq_entry

	.align 7 /* 0x500 -- FIQ */
	b	.Lirq_entry

	.align 7 /* 0x580 -- SError */
	ENTRY 1 arm_serror_handler

	# from aarch32 user
	.align 7 /* 0x600 -- Sync */
	ENTRY 1 arm_esr_entry

	.align 7 /* 0x680 -- IRQ */
	b	.Lirq_entry

	.align 7 /* 0x700 -- FIQ */
	b	.Lirq_entry

	.align 7 /* 0x780 -- SError */
	ENTRY 1 arm_serror_handler

	.align 7 /* 0x800 */
