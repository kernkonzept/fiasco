/* -*- c -*- */

/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 */

#include "globalconfig.h"
#include "asm_regs.h"
#include "asm_mips.h"

.section ".initcall.text", "ax"
.global _start
.ent _start
_start:
	mtc0	$0, $12, 0
	ehb
	mtc0	$0, $13, 0
	ehb
	ASM_LA	sp, __init_stack
	subu	sp, 4 * 4 /* a0 .. a3 */
	j	kernel_main
	  nop
.end _start

.macro MPLEAF name
	.align 2;
	.global	\name
	/* .ent	\name, 0 */
	\name\():
.endm

.macro MPEND name
	.align	2
	/* .end	\name */
	.global	\name\()_end
	\name\()_end:
.endm

.macro MPSNIPPET name
	.section .rodata.mp_snippet_\name, "a"
	MPLEAF \name
.endm

.set noreorder

#ifdef CONFIG_MP

/**
 * CPU initalization after reset.
 * \param v0  CCA that shall be used for KSEG0
 */
MPSNIPPET _tramp_mp_e0
	mtc0	$0, $13, 0 /* cause  <- 0 */
	mtc0	$0, $12, 0 /* status <- 0 */
	ehb
MPEND _tramp_mp_e0

MPSNIPPET _tramp_mp_segctl
	/* SegCtl for legacy */
	li	t0, 0x00200010
	mtc0	t0, $5, 2
	li	t0, 0x00030002
	ins	t0, v0, 16, 3 /* insert default CCA */
	mtc0	t0, $5, 3
	li	t0, 0x04330033
	ins	t0, v0, 16, 3 /* insert default CCA */
	ins	t0, v0, 0, 3  /* insert default CCA */
	mtc0	t0, $5, 4
MPEND _tramp_mp_segctl

MPSNIPPET _tramp_mp_cache
	mtc0	$0, $28, 0 /* TagLo */
	mtc0	$0, $29, 0 /* TagHi */
	mtc0	$0, $28, 2 /* TagLo */
	mtc0	$0, $29, 2 /* TagHi */
	ehb

	mfc0	t0, $16, 5
	li	t1, 1
	ins	t0, t1, 0, 30
	mtc0	t0, $16, 5
	ehb

	/* Initialize L1 caches */
	mfc0	t4, $16, 1

	ext	t0, t4, 19, 3
	beqz	t0, .Lic_done
	  li	t1, 2
	sllv	t0, t1, t0

	ext	t1, t4, 22, 3
	xori	t2, t1, 0x7
	beqz	t2, 1f
	  li	t3, 32
	addiu	t1, t1, 1
	sllv	t1, t3, t1
1:
	ext	t2, t4, 16, 3
	addiu	t2, t2, 1
	mul	t1, t1, t0
	mul	t1, t1, t2

	li	t5, 0x80000000 /* KSEG0 */
	addu	t6, t5, t1
1:	cache	0x08, 0(t5)
	addu	t5, t5, t0
	bne	t5, t6, 1b
	  nop
.Lic_done:

	ext	t0, t4, 10, 3
	beqz	t0, .Ldc_done
	  li	t1, 2
	sllv	t0, t1, t0

	ext	t1, t4, 13, 3
	xori	t2, t1, 0x7
	beqz	t2, 1f
	  li	t3, 32
	addiu	t1, t1, 1
	sllv	t1, t3, t1
1:
	ext	t2, t4, 7, 3
	addiu	t2, t2, 1
	mul	t1, t1, t0
	mul	t1, t1, t2

	li	t5, 0x80000000
	addu	t6, t5, t1
	subu	t6, t6, t0
1:	cache	0x9, 0(t5)
	bne	t5, t6, 1b
	  addu	t5, t5, t0
.Ldc_done:

	/* set K0 CCA = v0 */
	mfc0	t0, $16, 0
	ins	t0, v0, 0, 3
	mtc0	t0, $16, 0
	ehb
MPEND _tramp_mp_cache

/**
 * Enable coherency via CM2 API
 * \param v0  virtual, uncached CMGCR base address
 */
MPSNIPPET _tramp_mp_cm
	li	t0, 0xff
	sw	t0, 0x2008(v0)
	ehb
MPEND _tramp_mp_cm

/**
 * Snippet to jump t_tramp_mp_entry in cached memory
 */
MPSNIPPET _tramp_mp_jmp_entry
	ASM_LA	t1, _tramp_mp_entry
	jr	t1
	  nop
MPEND _tramp_mp_jmp_entry

/**
 * Application CPU stack setup.
 *
 * Blocks on the `_tramp_mp_spinlock` until no other CPU uses uses the
 * init stack anymore.
 */
.section .text.mp_tramp_entry, "ax"
MPLEAF _tramp_mp_entry
	ASM_LA	t1, _tramp_mp_spinlock
1:
	li	t2, 2
	sync
	ll	t0, (t1)
	bgtz	t0, 1b
	  nop
	sc	t2, (t1)
	beqz	t2, 1b
	  nop

	sync
	ASM_LA	sp, __init_stack
	subu	sp, 4 * 4 /* a0 .. a3 */
	j     BOOT_AP_CPU
	  nop
MPEND _tramp_mp_entry

.section ".bss.crt0", "aw", @nobits
#else
.section ".init.bss", "aw", @nobits
#endif
.align 12
	.space 4096
__init_stack:

#ifdef CONFIG_MP
.align 3
.global _tramp_mp_spinlock
_tramp_mp_spinlock: .long 0
#endif

.previous


