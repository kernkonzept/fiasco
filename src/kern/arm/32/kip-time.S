#include "globalconfig.h"
#include "kip_offsets.h"

.macro  read_timer_counter
#if defined(CONFIG_ARM_EM_TZ)
        /* Generic_timer::T<Physical>:
         * CNTKCTL: PL0VCTEN=1, PL0PCTEN=1.
         * Kernel uses physical counter.
         * Access to physical counter from PL0 allowed. */
        mrrc    p15, 0, r0, r1, c14
#else
        /* Generic_timer::T<Virtual>:
         *   CNTKCTL: PL0VCTEN=1, PL0PCTEN=0.
         *   Kernel uses virtual counter.
         *   Access to virtual counter from PL0 allowed.
         * Generic_timer::T<Hyp>, Generic_timer::T<Secure_hyp>:
         *   CNTKCTL: PL0VCTEN=1, PL0PCTEN=1.
         *   CNTHCTL: PL1PCEN=0, PL1PCTEN=0.
         *   CNTVOFF: 0
         *   Kernel uses physical counter. Virtual counter equals physical.
         *   Access to physical counter from non-secure PL0/PL1 prohibited.
         *   Access to virtual counter from non-secure PL0/PL1 allowed.
         */
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

.macro read_sync_and_convert scaler, shift
        ldr     ip, \scaler     /* ip = scaler */
        read_timer_counter
        umull   r0, r2, ip, r0  /* {r2,r0} = ts.lo * scaler */
        umull   r1, r3, ip, r1  /* {r3,r2} = (ts.hi * scaler) / 2^32 */
        ldr     ip, \shift      /* ip = shift */
        adds    r1, r1, r2
        adc     r2, r3, #0      /* {r2,r1,r0} contains (timer * scaler) */
        mov     r3, #32
        sub     r3, r3, ip      /* r3 = 32 - shift */
                                /* compute {r2,r1,r0} >> (32 - shift) */
        lsr     r0, r0, r3
        orr     r0, r0, r1, LSL ip /* r0 = (r1<<shift) | (r0 >> (32-shift)) */
        lsr     r1, r1, r3
        orr     r1, r1, r2, LSL ip /* r1 = (r2<<shift) | (r1 >> (32-shift)) */
.endm

        .section ".initcall.text", "ax"

/**
 * Provide the KIP clock value with a fine-grained resolution + accuracy.
 *
 * In case of CONFIG_SYNC_CLOCK is disabled, just provide the normal KIP
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
#define KIP_VAL(v) kip_time_fn_read_us + OFFS__ ##v - OFFS__KIP_FN_READ_US
kip_time_fn_read_us:
#ifdef CONFIG_SYNC_CLOCK
        read_sync_and_convert KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US),\
                              KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_US)
#else
        read_64bit KIP_VAL(KIP_CLOCK)
#endif
        bx      lr
#undef KIP_VAL
kip_time_fn_read_us_end:

/**
 * Read the ARM generic timer and and transform the value into nanoseconds.
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
#define KIP_VAL(v) kip_time_fn_read_ns + OFFS__ ##v - OFFS__KIP_FN_READ_NS
kip_time_fn_read_ns:
#ifdef CONFIG_SYNC_CLOCK
        read_sync_and_convert KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS),\
                              KIP_VAL(KIP_SHIFT_TIME_STAMP_TO_NS)
#else
        read_64bit KIP_VAL(KIP_CLOCK)
        mov     r3, #1000
        umull   r0, r2, r3, r0  /* {r2,r0} = r3 * r0 */
        umull   r1, ip, r3, r1  /* {ip,r1} = r3 * r1 */
        adds    r1, r1, r2
        /* Just ignore the upper few bits in ip/r12 and return {r1,r0}. */
#endif
        bx      lr
#undef KIP_VAL
kip_time_fn_read_ns_end:

