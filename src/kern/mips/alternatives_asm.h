#pragma once

/* feature bits from cpu-mips.cpp */
#define FEATURE_VZ     0x4
#define FEATURE_TLBINV 0x1

.macro ASM_ALTERNATIVE_ORIG_START
819:
.endm
.macro ASM_ALTERNATIVE_ORIG_END
829:
	.skip	-(((8991f - 8891f) - (829b - 819b)) > 0) * ((8991f - 8891f) - (829b - 819b)), 0x00
839:
.endm

.macro ASM_ALTERNATIVE_ALT_END alt=1
899\alt :
	.popsection
.endm

.macro ASM_ALTERNATIVE_ALT_START alt, feature
	.pushsection	.alt_insns, "a"
888:
	.long	819b - 888b
	.long	889\alt\()f  - 888b
	.word	\feature
	.byte	839b - 819b
	.byte	899\alt\()f  - 889\alt\()f
	.popsection
	.pushsection	.alt_insn_replacement, "ax"
889\alt :
.endm


.macro ALTERNATIVE_INSN orig_insn, new_insn, feature
	ASM_ALTERNATIVE_ORIG_START
	\orig_insn
	ASM_ALTERNATIVE_ORIG_END
	ASM_ALTERNATIVE_ALT_START	1, \feature
	\new_insn
	ASM_ALTERNATIVE_ALT_END	1
.endm

