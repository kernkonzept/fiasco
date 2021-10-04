#include "globalconfig.h"

#ifndef __mips64
# define OFFS__KIP_CLOCK                        0xA0
#else
# define OFFS__KIP_CLOCK                        0x140
#endif

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

        .section ".initcall.text", "ax"
        .set    noreorder

#define KIP_VAL(v)      KIP_OFFS(v)($t9)
#ifdef CONFIG_BIG_ENDIAN
# define KIP_VAL_LO(v)   4 + KIP_OFFS(v)($t9)
# define KIP_VAL_HI(v)   KIP_OFFS(v)($t9)
#else
# define KIP_VAL_LO(v)   KIP_OFFS(v)($t9)
# define KIP_VAL_HI(v)   4 + KIP_OFFS(v)($t9)
#endif

.macro read_barrier
# if defined(CONFIG_MP)
#  ifdef CONFIG_WEAK_ORDERING
#   ifdef CONFIG_LIGHTWEIGHT_BARRIERS
        sync    0x13
#   else
        sync
#   endif
#  endif
# endif
.endm

/**
 * Provide the normal KIP clock by reading the KIP clock field.
 * Assume that $t9 contains the function address according to the MIPS ABI.
 */
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
        .ent    kip_time_fn_read_us
#define KIP_OFFS(v)     OFFS__ ##v - OFFS__KIP_FN_READ_US
kip_time_fn_read_us:
#ifdef __mips64
        lw      $v0, KIP_VAL(KIP_CLOCK)
#else
1:      lw      $v1, KIP_VAL_HI(KIP_CLOCK)
        read_barrier
        lw      $v0, KIP_VAL_LO(KIP_CLOCK)
        read_barrier
        lw      $t0, KIP_VAL_HI(KIP_CLOCK)
        bne     $v1, $t0, 1b
          nop
#endif
        jr      $ra
          nop
#undef KIP_OFFS
          .end  kip_time_fn_read_us
kip_time_fn_read_us_end:

/**
 * Provide the normal KIP clock in nanoseconds by reading the KIP clock field
 * and multiply that value by 1000.
 * Assume that $t9 contains the function address according to the MIPS ABI.
 */
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
        .ent    kip_time_fn_read_ns
#define KIP_OFFS(v)     OFFS__ ##v - OFFS__KIP_FN_READ_NS
kip_time_fn_read_ns:
#ifdef __mips64
        lw      $v0, KIP_VAL(KIP_CLOCK)
        li      $t0, 1000
# ifdef _MIPS_ARCH_MIPS64R6
        dmulu   $v0, $t0, $v0
# else
        dmultu  $t0, $v0
        mflo    $v0
# endif
#else
1:      lw      $v1, KIP_VAL_HI(KIP_CLOCK)
        read_barrier
        lw      $v0, KIP_VAL_LO(KIP_CLOCK)
        read_barrier
        lw      $t0, KIP_VAL_HI(KIP_CLOCK)
        bne     $v1, $t0, 1b
          nop
                                // v0 = ts.lo
                                // v1 = ts.hi
        li      $t0, 1000       // t0 = 1000
# ifdef _MIPS_ARCH_MIPS32R6
        muhu    $t1, $v0, $t0   // t1 = (ts.lo * 1000).hi
        mulu    $v0, $v0, $t0   // v0 = (ts.lo * 1000).lo
        mulu    $v1, $v1, $t0   // v1 = (ts.hi * 1000).lo
# else
        multu   $t0, $v0        // hi:lo = ts.lo * 1000
        mflo    $v0             // v0 = (ts.lo * 1000).lo
        mfhi    $t1             // t1 = (ts.lo * 1000).hi
        multu   $t0, $v1        // hi:lo = ts.hi * 1000
        mflo    $v1             // v1 = (ts.hi * 1000).lo
# endif
                                // v0 = (ts.lo * 1000).lo
        add     $v1, $v1, $t1   // v1 = (ts.hi * 1000).lo + (ts.lo * 1000).hi
#endif
        jr      $ra
          nop
#undef KIP_OFFS
kip_time_fn_read_ns_end:
        .end    kip_time_fn_read_ns

#undef KIP_VAL
#undef KIP_VAL_LO
#undef KIP_VAL_HI
