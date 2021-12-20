#include "globalconfig.h"

#define OFFS__KIP_CLOCK                         0xA0

#define OFFS__KIP_SCALER_TIME_STAMP_TO_US       0x970
#define OFFS__KIP_SHIFT_TIME_STAMP_TO_US        0x978
#define OFFS__KIP_SCALER_TIME_STAMP_TO_NS       0x9F0
#define OFFS__KIP_SHIFT_TIME_STAMP_TO_NS        0x9F8

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

.macro  read_timer_counter
#if defined(CONFIG_CPU_VIRT)
        /* Generic_timer::T<Hyp>: R1=physical, R2=virtual:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=1.
         * CNTHCTL: PL1PCEN=0, PL1PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from non-secure PL0/PL1 allowed. */
        mrrc    p15, 0, r0, r1, c14
#elif defined(CONFIG_ARM_EM_TZ)
        /* Generic_timer::T<Physical>: R1=hyp, R2=virtual:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from PL0 allowed. */
        mrrc    p15, 0, r0, r1, c14
#else
        /* Generic_timer::T<Virtual>: R1=physical, R2=hyp:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=0.
         * Kernel uses virtual counter.
         * Access to virtual counter from PL0 allowed. */
        mrrc    p15, 1, r0, r1, c14
#endif
.endm

.macro read_barrier
#if defined(CONFIG_ARM_V6)
        mcr     p15, 0, r3, c7, c10, 5
#elif defined(CONFIG_ARM_V7)
        dmb     ish
#elif defined(CONFIG_ARM_V8PLUS)
        dmb     ishld
#endif
.endm

#ifdef CONFIG_BIG_ENDIAN
#error "Adapt read_64bit to big endian!"
#endif

/* Reads 64-bit value into r1:r0. Scrambles r2. */
.macro read_64bit mem
#if (defined(CONFIG_ARM_V7) && defined(CONFIG_ARM_LPAE)) || defined(CONFIG_ARM_V8PLUS)
        adrl    r2, \mem
        ldrd    r0, [r2]
#else
1:      ldr     r1, \mem + 4
        read_barrier
        ldr     r0, \mem
        read_barrier
        ldr     r2, \mem + 4
        cmp     r1, r2
        bne     1b
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
        push    {r4, r5, r6, r7}
        ldr     r6, KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US)  /* scaler */
        ldr     r7, KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_US)   /* shift */
        umull   r0, r3, r6, r0
        umull   r4, r5, r6, r1
        adds    r1, r4, r3
        adc     r2, r5, #0      /* {r2,r1,r0} contains (timer * scaler) */
        mov     r3, #32
        sub     r3, r3, r7
                                /* compute {r2,r1,r0} >> (32 - shift) */
        lsr     r0, r0, r3
        orr     r0, r0, r1, LSL r7 /* r0 = (r1<<shift) | (r0 >> (32-shift)) */
        lsr     r1, r1, r3
        orr     r1, r1, r2, LSL r7 /* r1 = (r2<<shift) | (r1 >> (32-shift)) */
        pop     {r4, r5, r6, r7}
#else
        read_64bit KIP_VAL(KIP_CLOCK)
#endif
        bx      lr
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
        push    {r4, r5, r6, r7}
        ldr     r6, KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        ldr     r7, KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_NS)
        umull   r0, r3, r6, r0
        umull   r4, r5, r6, r1
        adds    r1, r4, r3
        adc     r2, r5, #0
        mov     r3, #32
        sub     r3, r3, r7
        lsr     r0, r0, r3
        orr     r0, r0, r1, LSL r7
        lsr     r1, r1, r3
        orr     r1, r1, r2, LSL r7
        pop     {r4, r5, r6, r7}
#else
        read_64bit KIP_VAL(KIP_CLOCK)
        mov     r3, #1000
        umull   r0, r2, r3, r0  /* {r2,r0} = r3 * r0 */
        umull   r1, r12, r3, r1 /* {r12,r1} = r3 * r1 */
        adds    r1, r1, r2
        /* Just ignore the upper few bits in r12 and return {r1,r0}. */
#endif
        bx      lr
#undef KIP_VAL
kip_time_fn_read_ns_end:

