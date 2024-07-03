#include "globalconfig.h"
#include "tcboffset.h"

#include TRAMP_MP_ASM_INCLUDE

#define SCR_NS  (1 << 0)
#define SCR_SMD (1 << 7)
#define SCR_RW  (1 << 10)

#define HCR_HCD (1 << 29)
#define HCR_RW  (1 << 31)

#define SCR_EL3_DEFAULT_BITS (SCR_NS | SCR_SMD | SCR_RW)

#ifndef CONFIG_CPU_VIRT

#define HCR_EL2_DEFAULT_BITS (HCR_HCD | HCR_RW)

#endif

#ifndef HAVE_MACRO_BSP_EARLY_INIT
.macro bsp_early_init tmp1, tmp2
.endm
#endif

.macro defvar name
.global \name
\name: .8byte 0
.endm

	.section .mp_tramp_text, "ax"
	.p2align 2

cache_invalidate_l1:
	mrs     x0, CLIDR_EL1
	ands    x3, x0, #0x07000000
	orr     x3, xzr, x3, lsr #23
	b.eq    finished
	and     x1, x0, #7
	cmp     x1, #2
	b.lt    skip
	msr     CSSELR_EL1, xzr
	isb
	mrs     x1, CCSIDR_EL1
	and     x2, x1, #7
	add     x2, x2, #4
	movz    w4, #0x3ff
	ands    w4, w4, w1, lsr #3
	clz     w5, w4
	mov     x9, x4
loop1:
	movz    w7, #0x7fff
	ands    w7, w7, w1, lsr #13
loop2:
	lslv    x11, x9, x5
	lslv    x18, x7, x2
	orr     x11, x11, x18
	dc      isw, x11
	subs    x7, x7, #1
	b.ge    loop2
	subs    x9, x9, #1
	b.ge    loop1
skip:
	dsb     sy
finished:
	msr     CSSELR_EL1, xzr
	dsb     sy
	isb     sy
	ret

	.global _tramp_mp_entry
_tramp_mp_entry:
	msr	DAIFSet, #0xf
	bsp_early_init x0, x1
	mrs	x17, CurrentEL
	lsr	x17, x17, #2

	// EL3 is only available on A-profile
#ifdef CONFIG_ARM_PROFILE_A
	cmp	x17, #3 // running at EL3
	b.ne	.Lnot_el3

	// try to setup GIC for non-secure ...
	adrp	x16, _tramp_mp_gic_dist_base
	ldr     x16, [x16, :lo12:_tramp_mp_gic_dist_base]
	cmp	x16, #0
	b.eq	.Lno_gic
	mov	w4, #0xffffffff
	str	w4, [x16, 0x80]
	mov	x4, #0xff
	adrp	x14, _tramp_mp_gic_cpu_base
	ldr     x14, [x14, :lo12:_tramp_mp_gic_cpu_base]
	cmp	x14, #0
	b.eq	.Lgic_v3_cpu
	str	w4, [x14, 4]
	b	.Lno_gic
.Lgic_v3_cpu:
	msr	S3_0_C4_C6_0, x4
.Lno_gic:
	// switch from EL3 to EL2...
	tlbi	alle2is
	dsb	sy
	movz	x18, #:abs_g0_nc:SCR_EL3_DEFAULT_BITS
	movk	x18, #:abs_g1:SCR_EL3_DEFAULT_BITS
	msr	scr_el3, x18
	mrs	x20, sctlr_el3
	bl	cache_invalidate_l1
	and	x18, x20, #(1 << 25)
	.equ	sctlr_val, (  (3 << 4) | (1 << 11) | (1 << 16) \
		            | (1 << 18)| (3 << 22) | (3 << 28) \
		            | (1 << 2) | (1 << 1)  | (1 << 12))
	movz	x17, #:abs_g0_nc:sctlr_val
	movk	x17, #:abs_g1:sctlr_val
	orr	x18, x18, x17
#ifdef CONFIG_CPU_VIRT
	mov	x6, #HCR_RW
	msr	HCR_EL2, x6
	msr	sctlr_el2, x18
	mov	x18, #((0xf << 6) | 9) // => EL2
#else
	mrs	x6, id_aa64pfr0_el1
	ubfx	x6, x6, #8, #4
	cmp	x6, #0
	b.eq	1f
	mov	x6, #HCR_EL2_DEFAULT_BITS
	msr	HCR_EL2, x6
1:	msr	sctlr_el1, x18
	mov	x18, #((0xf << 6) | 5) // => EL1
#endif
	msr	spsr_el3, x18
	adr	x4, 1f
	msr	elr_el3, x4
	mov	x18, sp
	eret
1:	mov	sp, x18
	b	2f
.Lnot_el3:
#endif

	cmp	x17, #2
#ifndef CONFIG_CPU_VIRT
	b.ne  2f
	mrs	x6, MIDR_EL1
	msr	VPIDR_EL2, x6
	mrs	x6, MPIDR_EL1
	msr	VMPIDR_EL2, x6
	mov	x6, #HCR_EL2_DEFAULT_BITS
	msr	HCR_EL2, x6
	movz	x17, #((0xf << 6) | 5) // DAIF + EL1 / SPSEL
	msr	SPSR_EL2, x17
	adr	x18, 2f
	msr	ELR_EL2, x18
	eret
#else
	beq   2f
	switch_to_hyp
#endif
2:
	msr	DAIFSet, #0xf

	ic	iallu
	isb	sy
	dsb	sy

#if defined(CONFIG_MMU)
#ifdef CONFIG_CPU_VIRT
	// TLB flush
	tlbi	alle2
	tlbi	alle1

	adrp	x0, _tramp_mp_startup_tcr
	ldr	x0, [x0, :lo12:_tramp_mp_startup_tcr]
	msr	TCR_EL2, x0

	adrp	x0, _tramp_mp_startup_mair
	ldr	x0, [x0, :lo12:_tramp_mp_startup_mair]
	msr	MAIR_EL2, x0

	adrp	x0, _tramp_mp_startup_ttbr_kern
	ldr	x0, [x0, :lo12:_tramp_mp_startup_ttbr_kern]
	msr	TTBR0_EL2, x0
	dsb	sy

	adrp	x0, _tramp_mp_startup_sctlr
	ldr	x0, [x0, :lo12:_tramp_mp_startup_sctlr]
	msr	SCTLR_EL2, x0
#else
	tlbi	vmalle1

	msr	CONTEXTIDR_EL1, xzr

	adrp	x0, _tramp_mp_startup_tcr
	ldr	x1, [x0, :lo12:_tramp_mp_startup_tcr]
	msr	TCR_EL1, x1

	adrp	x0, _tramp_mp_startup_ttbr_usr
	ldr	x0, [x0, :lo12:_tramp_mp_startup_ttbr_usr]
	msr	TTBR0_EL1, x0

	adrp	x0, _tramp_mp_startup_ttbr_kern
	ldr	x0, [x0, :lo12:_tramp_mp_startup_ttbr_kern]
	msr	TTBR1_EL1, x0

	adrp	x0, _tramp_mp_startup_mair
	ldr	x0, [x0, :lo12:_tramp_mp_startup_mair]
	msr	MAIR_EL1, x0
	dsb	sy

	adrp x0, _tramp_mp_startup_sctlr
	ldr x0, [x0, :lo12:_tramp_mp_startup_sctlr]
	msr SCTLR_EL1, x0
#endif
	dsb	sy
	ic	iallu
	isb	sy

	ldr x0, 1f
	br x0
	.align 3
1:
	.8byte _tramp_mp_virt

	// we run paged now
_tramp_mp_virt:
#elif defined(CONFIG_MPU)
	ic	iallu

	ldr	x0, _tramp_mp_startup_mair
	ldr	x1, _tramp_mp_startup_prbar0
	ldr	x2, _tramp_mp_startup_prlar0

#ifdef CONFIG_CPU_VIRT
	msr	MAIR_EL2, x0
	msr	S3_4_c6_c2_1, xzr // PRSELR_EL2
	isb
	msr	S3_4_c6_c8_0, x1  // PRBAR_EL2
	msr	S3_4_c6_c8_1, x2  // PRLAR_EL2
#else
	msr	MAIR_EL1, x0
	msr	S3_0_c6_c2_1, xzr // PRSELR_EL1
	isb
	msr	S3_0_c6_c8_0, x1  // PRBAR_EL1
	msr	S3_0_c6_c8_1, x2  // PRLAR_EL1
#endif

	ldr	x0, _tramp_mp_startup_sctlr
	dsb	sy
#ifdef CONFIG_CPU_VIRT
	msr	SCTLR_EL2, x0
#else
	msr	SCTLR_EL1, x0
#endif
	isb
#endif

	// spinlock on cpu-init
	adrp	x0, _tramp_mp_spinlock
	add     x0, x0, :lo12:_tramp_mp_spinlock
	// Copy of Spin_lock::lock_arch(), we have no stack yet, so we cannot call it.
	// {
	b 2f
	prfm	pstl1strm, [x0]
1:	wfe
2:	ldaxr	x1, [x0]
	tst	x1, #2
	bne	1b
	orr	x2, x1, #2
	stxr	w1, x2, [x0]
	cbnz	w1, 1b
	// }

	adrp	x8, _tramp_mp_init_stack_top
	add     x8, x8, :lo12:_tramp_mp_init_stack_top
	mov	sp, x8
	ldr	x9, 3f
	br	x9

.align 4
3:
	.8byte BOOT_AP_CPU


	.section .mp_tramp_data, "aw"

#if defined(CONFIG_MMU)
	.align 12

.global _tramp_mp_boot_info
_tramp_mp_boot_info:
defvar _tramp_mp_startup_sctlr
defvar _tramp_mp_startup_tcr
defvar _tramp_mp_startup_mair
defvar _tramp_mp_startup_ttbr_kern
defvar _tramp_mp_startup_ttbr_usr
defvar _tramp_mp_gic_dist_base
defvar _tramp_mp_gic_cpu_base
#elif defined(CONFIG_MPU)
	.align 3
.global _tramp_mp_boot_info
_tramp_mp_boot_info:
defvar _tramp_mp_startup_sctlr
defvar _tramp_mp_startup_mair
defvar _tramp_mp_startup_prbar0
defvar _tramp_mp_startup_prlar0
#endif

.8byte 0
defvar _tramp_mp_spinlock

_tramp_mp_init_stack:
	.space 2048
_tramp_mp_init_stack_top:

