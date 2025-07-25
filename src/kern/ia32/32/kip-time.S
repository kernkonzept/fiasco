#include "globalconfig.h"

// See 'class Kip'
#define OFFS__KIP_CLOCK                         0x20

#define OFFS__KIP_SCALER_TIME_STAMP_TO_US       0x9F0
#define OFFS__KIP_SCALER_TIME_STAMP_TO_NS       0x9F8

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

        .section ".initcall.text", "ax"

#define KIP_VAL(v)      (KIP_OFFS(v))(%ebx)
#define KIP_VAL_LO(v)   (KIP_OFFS(v))(%ebx)
#define KIP_VAL_HI(v)   (4 + KIP_OFFS(v))(%ebx)

#ifdef CONFIG_BIG_ENDIAN
#error "Adapt KIP_VAL_LO/KIP_VAL_HI to big endian!"
#endif

// Provide the KIP clock value with a fine-grained resolution + accuracy.
//
// In case CONFIG_SYNC_CLOCK is disabled, just provide the normal KIP clock.
// Otherwise, read the time stamp counter and transform it into microseconds
// like done for the KIP clock value.
// This code will be copied to the KIP at OFFS__KIP_FN_READ_US.
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
#define KIP_OFFS(v)     OFFS__ ##v - OFFS__KIP_FN_READ_US - (2b - 1b)
kip_time_fn_read_us:
1:      push    %ebx
        call    2f
2:      pop     %ebx
#ifdef CONFIG_SYNC_CLOCK
        rdtsc
        movl    %edx, %ecx
        mull    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US)
        movl    %ecx, %eax
        movl    %edx, %ecx
        mull    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US)
        addl    %ecx, %eax
        adcl    $0, %edx
#else
3:      movl    KIP_VAL_HI(KIP_CLOCK), %edx
        movl    KIP_VAL_LO(KIP_CLOCK), %eax
        cmpl    %edx, KIP_VAL_HI(KIP_CLOCK)
        jne     3b
#endif
        pop     %ebx
        ret
#undef KIP_OFFS
kip_time_fn_read_us_end:


// Read the time stamp counter and transform it into nanoseconds.
//
// In case CONFIG_SYNC_CLOCK is enabled, provide the same value as the KIP clock
// but with nanoseconds resolution/accuracy. Otherwise, the provided time is
// not synchronized with the KIP clock but has still nanoseconds resolution.
// This code will be copied to the KIP at OFFS__KIP_FN_READ_NS.
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
#define KIP_OFFS(v)     OFFS__ ##v - OFFS__KIP_FN_READ_NS - (2b - 1b)
kip_time_fn_read_ns:
1:      push    %ebx
        call    2f
2:      pop     %ebx
#ifdef CONFIG_SYNC_CLOCK
        rdtsc
        push    %ebp
        movl    %edx, %ecx      // eax = tsc.lo, edx = tsc.hi
        mull    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        movl    %eax, %ebp      // ebp = (tsc.lo * scaler).lo
        movl    %ecx, %eax      // eax = tsc.hi
        movl    %edx, %ecx      // ecx = (tsc.lo * scaler).hi
        mull    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        addl    %ecx, %eax
        adcl    $0, %edx        // edx:eax:ebp = tsc * scaler
        shld    $5, %eax, %edx
        shld    $5, %ebp, %eax
        pop     %ebp            // edx:eax = (tsc * scaler) >> (32 - 5)
#else
3:      movl    KIP_VAL_HI(KIP_CLOCK), %edx
        movl    KIP_VAL_LO(KIP_CLOCK), %eax
        cmpl    %edx, KIP_VAL_HI(KIP_CLOCK)
        jne     3b
        movl    $1000, %ebx
        movl    %eax, %ecx      // ecx = clock.lo
        movl    %edx, %eax      // eax = clock.hi
        mull    %ebx            // eax = clock.hi * 1000, ignore bits in edx
        xchg    %ecx, %eax      // eax = clock.lo, ecx = (clock.hi * 1000).lo
        mull    %ebx            // edx:eax = clock.lo * 1000
        addl    %ecx, %edx      // (clock.lo * 1000).hi += (clock.hi * 1000).lo
#endif
        pop     %ebx
        ret
#undef KIP_OFFS
kip_time_fn_read_ns_end:
