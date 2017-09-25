#pragma once

.macro switch_to_hyp
	movw	r12, #0x102
	mov	r0, pc
	smc	#0
.endm
