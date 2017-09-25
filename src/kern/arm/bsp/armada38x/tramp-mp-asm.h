#pragma once

#define HAVE_MACRO_BSP_EARLY_INIT
.macro bsp_early_init tmp1, tmp2
        @ set SCU power to normal
        mrc   p15, 0, \tmp2, c0, c0, 5
        and   \tmp2, #3
        adr   \tmp1, .Lmpcore_phys_base
        ldr   \tmp1, [\tmp1]
        add   \tmp1, \tmp2
        mov   \tmp1, #0
        strb  \tmp1, [\tmp1, #8]
.endm
