#include "globalconfig.h"

#define OFFS__KIP_CLOCK                         0x140

#define OFFS__KIP_SCALER_TIME_STAMP_TO_US       0x970
#define OFFS__KIP_SHIFT_TIME_STAMP_TO_US        0x978
#define OFFS__KIP_SCALER_TIME_STAMP_TO_NS       0x9F0
#define OFFS__KIP_SHIFT_TIME_STAMP_TO_NS        0x9F8

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

.macro read_timer_counter
#if defined(CONFIG_CPU_VIRT)
        /* Generic_timer::T<Hyp>: R1=physical, R2=virtual:
         * CNTKCTL_EL1: EL0VTEN=1, EL0PTEN=1.
         * CNTHCTL_EL2: EL0VCTEN=0, EL0PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from non-secure PL0/PL1 allowed. */
        mrs     x0, CNTPCT_EL0
#elif defined(CONFIG_ARM_EM_TZ)
        /* Generic_timer::T<Physical>: R1=hyp, R2=virtual:
         * CNTKCTL_EL1: EL0VTEN=1, EL0PTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from PL0 allowed. */
        mrs     x0, CNTPCT_EL0
#else
        /* Generic_timer::T<Virtual>: R1=physical, R2=hyp:
         * CNTKCTL_EL1: EL0VTEN=1, EL0PTEN=0.
         * Kernel uses virtual counter.
         * Access to virtual counter from PL0 allowed. */
        mrs     x0, CNTVCT_EL0
#endif
.endm

        .section ".initcall.text", "ax"

/**
 * Provide the KIP clock value with a fine-grained resolution + accuracy.
 *
 * In case of CONFIG_ARM_SYNC_CLOCK is disabled, just provide the normal KIP
 * clock. Otherwise, read the ARM generic timer counter and transform it into
 * microseconds like done for the KIP clock value.
 * This code will be copied to the KIP at OFFS__KIP_FN_READ_US.
 *
 * The following formula is used to translate an ARM generic timer value into
 * a time value with microseconds resolution:
 *
 *                       timer value * scaler
 *   time(us, 64-bit) = ---------------------- * 2^shift
 *                               2^32
 */
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
#define KIP_VAL(v)      kip_time_fn_read_us + OFFS__ ##v - OFFS__KIP_FN_READ_US
kip_time_fn_read_us:
#ifdef CONFIG_ARM_SYNC_CLOCK
        read_timer_counter
        ldr     x4, KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US)  /* scaler */
        ldr     x5, KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_US)   /* shift */
        umulh   x1, x0, x4
        mul     x0, x0, x4      /* {x1,x0} contains (timer * scaler) */
        mov     x2, #32
        sub     x3, x2, x5
        add     x2, x2, x5
        lsr     x0, x0, x3      /* x0 = x0 >> (32 - shift) */
        lsl     x1, x1, x2      /* x1 = x1 << (32 + shift) */
        orr     x0, x0, x1      /* OR the "out-shifted" bits to r0 */
#else
        ldr     x0, KIP_VAL(KIP_CLOCK)
#endif
        ret
#undef KIP_VAL
kip_time_fn_read_us_end:

/**
 * Read the ARM generic timer and and transform the value into nanoseconds.
 *
 * In case of CONFIG_ARM_SYNC_CLOCK is enabled, provide the same value as the
 * KIP clock but with nanoseconds resolution/accuracy. Otherwise, read the KIP
 * clock and return that value multiplied by 1000.
 * This code will be copied to the KIP at OFFS__KIP_FN_READ_NS.
 *
 * Actually the very same code as kip_time_fn_read_us() just with different
 * scaler and shift values.
 */
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
#define KIP_VAL(v)      kip_time_fn_read_ns + OFFS__ ##v - OFFS__KIP_FN_READ_NS
kip_time_fn_read_ns:
#ifdef CONFIG_ARM_SYNC_CLOCK
        read_timer_counter
        ldr     x4, KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        ldr     x5, KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_NS)
        umulh   x1, x0, x4
        mul     x0, x0, x4
        mov     x2, #32
        sub     x3, x2, x5
        add     x2, x2, x5
        lsr     x0, x0, x3
        lsl     x1, x1, x2
        orr     x0, x0, x1
#else
        ldr     x0, KIP_VAL(KIP_CLOCK)
        mov     x1, #1000
        mul     x0, x0, x1
#endif
        ret
#undef KIP_VAL
kip_time_fn_read_ns_end:

