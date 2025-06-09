#include "globalconfig.h"

# define OFFS__KIP_CLOCK                        0x20

#define OFFS__KIP_SCALER_TIME_STAMP_TO_US       0x970
#define OFFS__KIP_SHIFT_TIME_STAMP_TO_US        0x978
#define OFFS__KIP_SCALER_TIME_STAMP_TO_NS       0x9F0
#define OFFS__KIP_SHIFT_TIME_STAMP_TO_NS        0x9F8

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

.macro read_time_and_convert scaler shift
# if __riscv_xlen == 32
read_again\@:
        rdtimeh a1  /* time.hi */
        rdtime  a0  /* time.lo */
        rdtimeh t0
        bne     a1, t0, read_again\@
        lw      a3, \scaler  /* scaler */
        lw      a4, \shift   /* shift */

        /* compute (time * scaler) */
        mulhu   t0, a0, a3
        mul     a0, a0, a3  /* {t0,a0} contains (time.lo * scaler) */
        mulhu   t1, a1, a3
        mul     a1, a1, a3  /* {t1,a1} contains (time.hi * scaler) */
        add     a1, a1, t0
        sltu    t2, a1, t0  /* sltu + add is an add with carry from a1 + t0 */
        add     a2, t1, t2  /* {a2,a1,a0} contains (time * scaler) */

        /* compute {a2,a1,a0} >> (32 - shift) */
        li      t0, 31      /* RISC-V only supports 5-bit shifts. Given that 0 <= shift <= 31, */
        sub     t0, t0, a4  /* we can use a second constant shift-instruction as workaround. */
        sll     t1, a1, a4
        sll     t2, a2, a4
        srli    a0, a0, 1
        srl     a0, a0, t0
        or      a0, a0, t1
        srli    a1, a1, 1
        srl     a1, a1, t0
        or      a1, a1, t2
# else
        rdtime  a0
        ld      a2, \scaler  /* scaler */
        ld      a3, \shift   /* shift */

        mulhu   a1, a0, a2
        mul     a0, a0, a2  /* {a1,a0} contains (timer * scaler) */

        li      t0, 32
        subw    t1, t0, a3
        addw    t0, t0, a3
        srl     a0, a0, t1  /* a0 = a0 >> (32 - shift) */
        sll     a1, a1, t0  /* a1 = a1 << (32 + shift) */
        or      a0, a0, a1  /* OR the "out-shifted" bits to a0 */
# endif
.endm

/**
 * Read the "time" CSR and transform the value into microseconds.
 *
 * In case of CONFIG_SYNC_CLOCK is disabled, just provide the normal KIP
 * clock. Otherwise, read the "time" CSR and transform it into microseconds like
   done for the KIP clock value.
 * This code will be copied to the KIP at OFFS__KIP_FN_READ_US.
 *
 * The following formula is used to translate a "time" CSR value into a time
   value with microseconds resolution:
 *
 *                       timer value * scaler
 *   time(us, 64-bit) = ---------------------- * 2^shift
 *                               2^32
 */
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
#define KIP_OFFS(v)      kip_time_fn_read_us + OFFS__ ##v - OFFS__KIP_FN_READ_US
kip_time_fn_read_us:
#ifdef CONFIG_SYNC_CLOCK
        read_time_and_convert KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US), KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_US)
#else
#  if __riscv_xlen == 32
1:      lw      a1, KIP_VAL_HI(KIP_CLOCK)
        read_barrier
        lw      a0, KIP_VAL_LO(KIP_CLOCK)
        read_barrier
        lw      t0, KIP_VAL_HI(KIP_CLOCK)
        bne     a1, t0, 1b
#  else
        ld      a0, KIP_VAL(KIP_CLOCK)
#  endif
#endif
        ret
#undef KIP_OFFS
kip_time_fn_read_us_end:

/**
 * Provide the normal KIP clock in nanoseconds by reading the KIP clock field
 * and multiply that value by 1000.
 */
 /**
 * Read the "time" CSR and transform the value into nanoseconds.
 *
 * In case of CONFIG_SYNC_CLOCK is enabled, provide the same value as the
 * KIP clock but with nanoseconds resolution/accuracy. Otherwise, read the KIP
 * clock and return that value multiplied by 1000.
 * This code will be copied to the KIP at OFFS__KIP_FN_READ_NS.
 *
 * Actually the very same code as kip_time_fn_read_us() just with different
 * scaler and shift values.
 */
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
#define KIP_OFFS(v)      kip_time_fn_read_ns + OFFS__ ##v - OFFS__KIP_FN_READ_NS
kip_time_fn_read_ns:
#ifdef CONFIG_SYNC_CLOCK
        read_time_and_convert KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS), KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_NS)
#else
#  if __riscv_xlen == 32
1:      lw      a1, KIP_VAL_HI(KIP_CLOCK)
        read_barrier
        lw      a0, KIP_VAL_LO(KIP_CLOCK)
        read_barrier
        lw      t0, KIP_VAL_HI(KIP_CLOCK)
        bne     a1, t0, 1b
        li      t0, 1000    /* t0 = 1000 */
        mulhu   t1, a0, t0  /* t1 = (ts.lo * 1000).hi */
        mul     a1, a1, t0  /* a1 = (ts.hi * 1000).lo */
        mul     a0, a0, t0  /* a0 = (ts.lo * 1000).lo */
        add     a1, a1, t1  /* a1 = (ts.hi * 1000).lo + (ts.lo * 1000).hi */
#  else
        ld      a0, KIP_VAL(KIP_CLOCK)
        li      t0, 1000
        mul     a0, a0, t0
#  endif
#endif
        ret
#undef KIP_OFFS
kip_time_fn_read_ns_end:

#undef KIP_VAL
#undef KIP_VAL_LO
#undef KIP_VAL_HI

        .option pop
