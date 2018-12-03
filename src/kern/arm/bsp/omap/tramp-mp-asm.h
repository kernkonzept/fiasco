#pragma once

.macro switch_to_hyp
	.arch_extension sec
	movw	r12, #0x102
	mov	r0, pc
	smc	#0
.endm
