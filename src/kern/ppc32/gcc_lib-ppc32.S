/*
 * Copyright (C) 2026 Kernkonzept GmbH.
 * Author(s): Martin Decky <martin.decky@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include "fiasco_defs.h"

.section .text

#define FN_HEADER(name) \
	.type name, @function; \
	.globl name; \
	.hidden name; \
	name:

#define FN_FOOTER(name) \
	FIASCO_MACRO_JOIN(.L, name): \
	.size name, FIASCO_MACRO_JOIN(.L, name) - name

.cfi_startproc
.cfi_def_cfa_register 11
.cfi_offset 65, 4
.cfi_offset 14, -72
.cfi_offset 15, -68
.cfi_offset 16, -64
.cfi_offset 17, -60
.cfi_offset 18, -56
.cfi_offset 19, -52
.cfi_offset 20, -48
.cfi_offset 21, -44
.cfi_offset 22, -40
.cfi_offset 23, -36
.cfi_offset 24, -32
.cfi_offset 25, -28
.cfi_offset 26, -24
.cfi_offset 27, -20
.cfi_offset 28, -16
.cfi_offset 29, -12
.cfi_offset 30, -8
.cfi_offset 31, -4

/*
 * The following functions are required by GCC to restore the values of GPRs.
 * On entry, the register 11 contains the address of the GPR save area plus
 * 4 bytes.
 */

FN_HEADER(_restgpr_14_x)
	lwz 14, -72(11)
	.cfi_restore 14

FN_HEADER(_restgpr_15_x)
	lwz 15, -68(11)
	.cfi_restore 15

FN_HEADER(_restgpr_16_x)
	lwz 16, -64(11)
	.cfi_restore 16

FN_HEADER(_restgpr_17_x)
	lwz 17, -60(11)
	.cfi_restore 17

FN_HEADER(_restgpr_18_x)
	lwz 18, -56(11)
	.cfi_restore 18

FN_HEADER(_restgpr_19_x)
	lwz 19, -52(11)
	.cfi_restore 19

FN_HEADER(_restgpr_20_x)
	lwz 20, -48(11)
	.cfi_restore 20

FN_HEADER(_restgpr_21_x)
	lwz 21, -44(11)
	.cfi_restore 21

FN_HEADER(_restgpr_22_x)
	lwz 22, -40(11)
	.cfi_restore 22

FN_HEADER(_restgpr_23_x)
	lwz 23, -36(11)
	.cfi_restore 23

FN_HEADER(_restgpr_24_x)
	lwz 24, -32(11)
	.cfi_restore 24

FN_HEADER(_restgpr_25_x)
	lwz 25, -28(11)
	.cfi_restore 25

FN_HEADER(_restgpr_26_x)
	lwz 26, -24(11)
	.cfi_restore 26

FN_HEADER(_restgpr_27_x)
	lwz 27, -20(11)
	.cfi_restore 27

FN_HEADER(_restgpr_28_x)
	lwz 28, -16(11)
	.cfi_restore 28

FN_HEADER(_restgpr_29_x)
	lwz 29, -12(11)
	.cfi_restore 29

FN_HEADER(_restgpr_30_x)
	lwz 30, -8(11)
	.cfi_restore 30

FN_HEADER(_restgpr_31_x)
	lwz 0, 4(11)
	lwz 31, -4(11)
	.cfi_restore 31

	mtlr 0
	.cfi_restore 65

	mr 1, 11
	.cfi_def_cfa_register 1

	blr

FN_FOOTER(_restgpr_31_x)
FN_FOOTER(_restgpr_30_x)
FN_FOOTER(_restgpr_29_x)
FN_FOOTER(_restgpr_28_x)
FN_FOOTER(_restgpr_27_x)
FN_FOOTER(_restgpr_26_x)
FN_FOOTER(_restgpr_25_x)
FN_FOOTER(_restgpr_24_x)
FN_FOOTER(_restgpr_23_x)
FN_FOOTER(_restgpr_22_x)
FN_FOOTER(_restgpr_21_x)
FN_FOOTER(_restgpr_20_x)
FN_FOOTER(_restgpr_19_x)
FN_FOOTER(_restgpr_18_x)
FN_FOOTER(_restgpr_17_x)
FN_FOOTER(_restgpr_16_x)
FN_FOOTER(_restgpr_15_x)
FN_FOOTER(_restgpr_14_x)

.cfi_endproc
