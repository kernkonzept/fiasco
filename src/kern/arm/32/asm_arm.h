#pragma once

#if !defined(__ARM_ARCH) || (__ARM_ARCH < 7)
.macro dsb scope
	mcr	p15, 0, r0, c7, c10, 4  // CP15DSB
.endm
.macro isb scope
	mcr	p15, 0, r0, c7, c5, 4   // CP15ISB
.endm

#endif
