#include "globalconfig.h"

#if __riscv_xlen == 32
# define OFFS__KIP_CLOCK                        0xA0
#else
# define OFFS__KIP_CLOCK                        0x140
#endif

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

        .section ".initcall.text", "ax"
        /* Disable relaxation against the global pointer (gp register), as the
          following functions are executed outside of Fiasco in user mode. */
        .option push
        .option norelax

#define KIP_VAL(v)      KIP_OFFS(v)
#define KIP_VAL_LO(v)   KIP_OFFS(v)
#define KIP_VAL_HI(v)   4 + KIP_OFFS(v)

.macro read_barrier
# if defined(CONFIG_MP)
        fence r, r
# endif
.endm

/**
 * Provide the normal KIP clock by reading the KIP clock field.
 */
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
#define KIP_OFFS(v)      kip_time_fn_read_us + OFFS__ ##v - OFFS__KIP_FN_READ_US
kip_time_fn_read_us:
#if __riscv_xlen == 32
1:      lw      a1, KIP_VAL_HI(KIP_CLOCK)
        read_barrier
        lw      a0, KIP_VAL_LO(KIP_CLOCK)
        read_barrier
        lw      t0, KIP_VAL_HI(KIP_CLOCK)
        bne     a1, t0, 1b
#else
        ld      a0, KIP_VAL(KIP_CLOCK)
#endif
        ret
#undef KIP_OFFS
kip_time_fn_read_us_end:

/**
 * Provide the normal KIP clock in nanoseconds by reading the KIP clock field
 * and multiply that value by 1000.
 */
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
#define KIP_OFFS(v)      kip_time_fn_read_ns + OFFS__ ##v - OFFS__KIP_FN_READ_NS
kip_time_fn_read_ns:
#if __riscv_xlen == 32
1:      lw      a1, KIP_VAL_HI(KIP_CLOCK)
        read_barrier
        lw      a0, KIP_VAL_LO(KIP_CLOCK)
        read_barrier
        lw      t0, KIP_VAL_HI(KIP_CLOCK)
        bne     a1, t0, 1b
        li      t0, 1000     // t0 = 1000
        mulhu   t1, a0, t0   // t1 = (ts.lo * 1000).hi
        mul     a1, a1, t0   // a1 = (ts.hi * 1000).lo
        mul     a0, a0, t0   // a0 = (ts.lo * 1000).lo
        add     a1, a1, t1   // a1 = (ts.hi * 1000).lo + (ts.lo * 1000).hi
#else
        ld      a0, KIP_VAL(KIP_CLOCK)
        li      t0, 1000
        mul     a0, a0, t0
#endif
        ret
#undef KIP_OFFS
kip_time_fn_read_ns_end:

#undef KIP_VAL
#undef KIP_VAL_LO
#undef KIP_VAL_HI

        .option pop
