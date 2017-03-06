#include "globalconfig.h"
#include "tcboffset.h"

#include TRAMP_MP_ASM_INCLUDE

#ifndef HAVE_MACRO_BSP_EARLY_INIT
.macro bsp_early_init tmp1, tmp2
.endm
#endif

.macro defvar name
.global \name
\name: .8byte 0
.endm

	.section .mp_tramp, "awx"
	.p2align 12

#ifdef CONFIG_ARM_V8
cache_invalidate_v8:
	// B2.2.7
	mrs     x0, CLIDR_EL1
	ands    x3, x0, #0x07000000
	orr     x3, xzr, x3, lsr #23
	b.eq    finished
	mov     x10, #0
loop1:
	add     x2, x10, x10, lsr #1
	lsrv    x1, x0, x2
	and     x1, x1, #7
	cmp     x1, #2
	b.lt    skip
	msr     CSSELR_EL1, x10
	isb
	mrs     x1, CCSIDR_EL1
	and     x2, x1, #7
	add     x2, x2, #4
	movz    w4, #0x3ff
	ands    w4, w4, w1, lsr #3
	clz     w5, w4
	mov     x9, x4
loop2:
	movz    w7, #0x7fff
	ands    w7, w7, w1, lsr #13
loop3:
	lslv    x17, x9, x5
	orr     x11, x10, x17
	lslv    x18, x7, x2
	orr     x11, x11, x18
	dc      isw, x11
	subs    x7, x7, #1
	b.ge    loop3
	subs    x9, x9, #1
	b.ge    loop2
skip:
	add     x10, x10, #2
	cmp     x3, x10
	b.gt    loop1
	dsb     sy
finished:
	msr     CSSELR_EL1, xzr
	dsb     sy
	isb     sy
	ret
#endif

	.global _tramp_mp_entry
_tramp_mp_entry:
	msr	DAIFSet, #0xf
	bsp_early_init x0, x1
	mrs	x17, CurrentEL
	lsr	x17, x17, #2
	cmp	x17, #3 // running at EL3
	b.ne	.Lnot_el3

	// try to setup GIC for non-secure ...
	ldr	x16, _tramp_mp_gic_dist_base
	cmp	x16, #0
	b.eq	.Lno_gic
	mov	w4, #0xffffffff
	str	w4, [x16, 0x80]
	ldr	x14, _tramp_mp_gic_cpu_base
	mov	w4, #0xf0
	str	w4, [x14, 4]
.Lno_gic:
	// switch from EL3 to EL2...
	tlbi	alle2is
	dsb	sy
	mov	x18, #((1 << 0) | (1 << 7) | (1 << 10))
	msr	scr_el3, x18
	mrs	x20, sctlr_el3
	bl	cache_invalidate_v8
	and	x18, x20, #(1 << 25)
	.equ	sctlr_val, (  (3 << 4) | (1 << 11) | (1 << 16) \
		            | (1 << 18)| (3 << 22) | (3 << 28) \
		            | (1 << 2) | (1 << 1)  | (1 << 12))
	movz	x17, #:abs_g0_nc:sctlr_val
	movk	x17, #:abs_g1:sctlr_val
	orr	x18, x18, x17
#ifdef CONFIG_CPU_VIRT
	mov	x6, #(1<< 31) // RW: EL1 = 64bit
	msr	HCR_EL2, x6
	msr	sctlr_el2, x18
	mov	x18, #((0xf << 6) | 9) // => EL2
#else
	mrs	x6, id_aa64pfr0_el1
	ubfx	x6, x6, #8, #4
	cmp	x6, #0
	b.eq	1f
	mov	x6, #((1 << 29) | (1<< 31)) // RW + HCD
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
	cmp	x17, #2
#ifndef CONFIG_CPU_VIRT
	b.ne  2f
	mov	x6, #((1 << 29) | (1<< 31)) // RW + HCD
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

	// enable SMP
	mrs	x0, S3_1_C15_C2_1 // CPUECTRL_EL1
	tbnz	w0, #6, 5f
	orr	x0, x0, #(1 << 6)
	msr	S3_1_C15_C2_1, x0 // CPUECTRL_EL1
5:
	bl	cache_invalidate_v8

	ic	iallu
	isb	sy
	dsb	sy

#ifdef CONFIG_CPU_VIRT
	// TLB flush
	tlbi	alle2
	tlbi	alle1

	adr	x0, _tramp_mp_startup_tcr
	ldr	x0, [x0]
	msr	TCR_EL2, x0

	adr	x0, _tramp_mp_startup_mair
	ldr	x0, [x0]
	msr	MAIR_EL2, x0

	adr	x0, _tramp_mp_startup_ttbr_kern
	ldr	x0, [x0]
	msr	TTBR0_EL2, x0
	dsb	sy

	adr	x0, _tramp_mp_startup_sctlr
	ldr	x0, [x0]
	msr	SCTLR_EL2, x0
#else
	tlbi	vmalle1

	msr	CONTEXTIDR_EL1, xzr

	adr	x0, _tramp_mp_startup_tcr
	ldr	x1, [x0]
	msr	TCR_EL1, x1

	adr	x0, _tramp_mp_startup_ttbr_usr
	ldr	x0, [x0]
	msr	TTBR0_EL1, x0

	adr	x0, _tramp_mp_startup_ttbr_kern
	ldr	x0, [x0]
	msr	TTBR1_EL1, x0

	adr	x0, _tramp_mp_startup_mair
	ldr	x0, [x0]
	msr	MAIR_EL1, x0
	dsb	sy

	adr x0, _tramp_mp_startup_sctlr
	ldr x0, [x0]
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

	.align 3
.global _tramp_mp_boot_info
_tramp_mp_boot_info:
defvar _tramp_mp_startup_sctlr
defvar _tramp_mp_startup_tcr
defvar _tramp_mp_startup_mair
defvar _tramp_mp_startup_ttbr_kern
defvar _tramp_mp_startup_ttbr_usr
defvar _tramp_mp_gic_dist_base
defvar _tramp_mp_gic_cpu_base

	// we run paged now
_tramp_mp_virt:
	// spinlock on cpu-init
	adr	x0, _tramp_mp_spinlock
1:	ldr	x1, [x0]
	cbz	x1, 2f
	wfe
	b	1b

2:	ldxr	x1, [x0]
	cbnz	x1, 1b
	mov	x1, #2
	stxr	w2, x1, [x0]
	cbnz	w2, 1b

	adr	x8, _tramp_mp_init_stack_top
	mov	sp, x8
	ldr	x9, 3f
	br	x9

.align 4
3:
	.8byte BOOT_AP_CPU

.8byte 0
defvar _tramp_mp_spinlock

_tramp_mp_init_stack:
	.space 2048
_tramp_mp_init_stack_top:

