// See 'class Kip'
#define OFFS__KIP_CLOCK                         0xA0

#define OFFS__KIP_SCALER_TIME_STAMP_TO_US       0x9F0
#define OFFS__KIP_SCALER_TIME_STAMP_TO_NS       0x9F8

#define OFFS__KIP_FN_READ_US                    0x900
#define OFFS__KIP_FN_READ_NS                    0x980

        .section ".initcall.text", "ax"

#define KIP_VAL(v)      (KIP_OFFS(v))(%ebx)
#define KIP_VAL_LO(v)   (KIP_OFFS(v))(%ebx)
#define KIP_VAL_HI(v)   (4 + KIP_OFFS(v))(%ebx)

// Read us from time stamp counter and synchronize with KIP.
// Located at KIP.OFFS__KIP_FN_READ_US
        .global kip_time_fn_read_us
        .global kip_time_fn_read_us_end
#define KIP_OFFS(v)     OFFS__ ##v - OFFS__KIP_FN_READ_US - (2b - 1b)
kip_time_fn_read_us:
1:      push    %ebx
        call    2f
2:      pop     %ebx
3:      movl    KIP_VAL_HI(KIP_CLOCK), %edx
        movl    KIP_VAL_LO(KIP_CLOCK), %eax
        cmpl    %edx, KIP_VAL_HI(KIP_CLOCK)
        jne     3b
        pop     %ebx
        ret
#undef KIP_OFFS
kip_time_fn_read_us_end:


// Read ns from time stamp counter.
// Located at KIP.OFFS__KIP_FN_READ_NS
        .global kip_time_fn_read_ns
        .global kip_time_fn_read_ns_end
#define KIP_OFFS(v)     OFFS__ ##v - OFFS__KIP_FN_READ_NS - (2b - 1b)
kip_time_fn_read_ns:
1:      push    %ebx
        call    2f
2:      pop     %ebx
        rdtsc
        movl    %edx, %ecx
        mull    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        movl    %ecx, %eax
        movl    %edx, %ecx
        mull    KIP_VAL(KIP_SCALER_TIME_STAMP_TO_NS)
        addl    %ecx, %eax
        adcl    $0, %edx
        shld    $5, %eax, %edx
        shll    $5, %eax
        pop     %ebx
        ret
#undef KIP_OFFS
kip_time_fn_read_ns_end:
