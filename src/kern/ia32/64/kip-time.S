#include "globalconfig.h"

// See 'class Kip'
#define OFFS__KIP_CLOCK                         0x140

#define OFFS__KIP_SCALER_TIME_STAMP_TO_US       0x9F0
#define OFFS__KIP_SCALER_TIME_STAMP_TO_NS       0x9F8

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

        .section ".initcall.text", "ax"

#define KIP_VAL(v)      (KIP_OFFS(v))(%rip)

// Provide the KIP clock value with a fine-grained resolution + accuracy.
//
// In case CONFIG_SYNC_TSC is disabled, just provide the normal KIP clock.
// Otherwise, read the time stamp counter and transform it into microseconds
// like done for the KIP clock value.
// This code will be copied to the KIP at OFFS__KIP_FN_READ_US.
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
#define KIP_OFFS(v)     1b - OFFS__KIP_FN_READ_US + OFFS__ ##v
kip_time_fn_read_us:
1:
#ifdef CONFIG_SYNC_TSC
        rdtsc
        and     $0xffffffff, %eax       // that's actually not necessary
        shl     $32, %rdx
        or      %rdx, %rax
        mulq    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_US)
        shrd    $32, %rdx, %rax
#else
        movq    KIP_VAL(KIP_CLOCK), %rax
#endif
        ret
#undef KIP_OFFS
kip_time_fn_read_us_end:

// Read the time stamp counter and transform it into nanoseconds.
//
// In case CONFIG_SYNC_TSC is enabled, provide the same value as the KIP clock
// but with nanoseconds resolution/accuracy. Otherwise, the provided time is
// not synchronized with the KIP clock but has still nanoseconds resolution.
// This code will be copied to the KIP at OFFS__KIP_FN_READ_NS.
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
#define KIP_OFFS(v)     1b - OFFS__KIP_FN_READ_NS + OFFS__ ##v
kip_time_fn_read_ns:
1:      rdtsc
        and     $0xffffffff, %eax       // that's actually not necessary
        shl     $32, %rdx
        or      %rdx, %rax
        mulq    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        shrd    $27, %rdx, %rax
        ret
#undef KIP_OFFS
kip_time_fn_read_ns_end:
