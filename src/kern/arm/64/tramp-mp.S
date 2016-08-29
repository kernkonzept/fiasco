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
        bsp_early_init x0, x1
        mrs   x17, CurrentEL
        cmp   x17, #0x8 // hyp mode
#ifndef CONFIG_CPU_VIRT
        b.ne  2f

	movz	x17, #((0xf << 6) | 0x5) // DAIF + EL1 / SPSEL
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
#ifndef CONFIG_ARM_EM_NS
	adr   x0, .Lmpcore_phys_base
	ldr   x0, [x0]
	ldr   x1, [x0]
	orr   x1, x1, #1
	str   x1, [x0]

	DC	cvac, x0
	dsb	sy

#endif

#ifdef CONFIG_ARM_V8
	bl    cache_invalidate_v8
#endif

	ic	iallu
	isb	sy
	dsb	sy
#if 0
#ifdef CONFIG_ARM_V7
	// ACTRL is implementation defined
	mrc   p15, 0, r0, c0, c0, 0  // read MIDR
	adr   r3, .Lactrl_cpuid      // load addr
	ldm   r3, {r1, r2}           // load mask + val
	and   r3, r0, r1             // apply mask
	teq   r3, r2                 // check value
	bne   2f                     // only do mcr on this CPU
#endif

	mrc   p15, 0, r1, c1, c0, 1
#ifdef CONFIG_ARM_V7
	tst   r1, #0x40
	bne   2f
	lsr   r0, r0, #8
	and   r0, r0, #7
	cmp   r0, #7
	orrne r1, r1, #0x41
	orreq r1, r1, #0x40
#else
	orr   r1, r1, #0x20
#endif
	mcr   p15, 0, r1, c1, c0, 1

2:
#endif
#ifdef CONFIG_CPU_VIRT
        // TLB flush
        tlbi	alle2
        tlbi	alle1

        // init TTBCR
        adr	x0, _tramp_mp_startup_ttbcr
        ldr	x1, [x0]
	msr	TTBCR_EL2, x1

        adr	x0, _tramp_mp_startup_pdbr
        ldr	x0, [x0]
	msr	TTBR0_EL2, x0
	movz	x0, #(1 | 2 | 4 | 16 | 32 | 0x1000)
	msr	SCTLR_EL2, x0

#else
	tlbi	alle1

	// reset ASID and PROCID
	mov	x0, #0
	msr	CONTEXTIDR_EL1, x0

	// init TTBCR
	adr	x0, _tramp_mp_startup_ttbcr
	ldr	x1, [x0]
	msr	TCR_EL1, x1

	mov x0, #0
	tlbi	alle1

	adr	x0, _tramp_mp_startup_pdbr
	ldr	x0, [x0]
	msr	TTBR0_EL1, x0
#endif

	adr x0, _tramp_mp_startup_sctlr
	ldr x0, [x0]
	msr SCTLR_EL1, x0

	// barrier
	isb sy
	ldr x0, 1f
	br x0
1:
	.8byte _tramp_mp_virt

.Lmpcore_phys_base:
	.8byte MPCORE_PHYS_BASE

.Lactrl_cpuid:
	.long 0xff0fff00
	.long 0x410fc000

.global _tramp_mp_boot_info
_tramp_mp_boot_info:
defvar _tramp_mp_startup_sctlr
defvar _tramp_mp_startup_pdbr
defvar _tramp_mp_startup_ttbcr

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

	// TLB flush
	tlbi	alle1

	adr	x8, _tramp_mp_init_stack_top
	mov	sp, x8
	ldr	x9, 3f
	br	x9
3:
	.8byte BOOT_AP_CPU

.8byte 0
defvar _tramp_mp_spinlock

_tramp_mp_init_stack:
	.space 2048
_tramp_mp_init_stack_top:

