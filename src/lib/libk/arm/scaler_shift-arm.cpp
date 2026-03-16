IMPLEMENTATION [arm && 32bit]:

IMPLEMENT
Unsigned64
Scaler_shift::transform(Unsigned64 value) const
{
  Mword lo = value & 0xffffffffU;
  Mword hi = value >> 32;
  Mword dummy1, dummy2, dummy3;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umull   %0, %2, %[scaler], %0   \n\t"
       "umull   %1, %3, %[scaler], %1   \n\t"
       "adds    %1, %1, %2              \n\t"
       "adc     %2, %3, #0              \n\t"
       "rsb     %3, %[shift], #32       \n\t"
#ifdef __thumb__
       "lsl     %4, %1, %[shift]        \n\t"
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %4              \n\t"
       "lsl     %4, %2, %[shift]        \n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %4              \n\t"
#else
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %1, LSL %[shift]\n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %2, LSL %[shift]\n\t"
#endif
       : "+r"(lo), "+r"(hi),
         "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3)
       : [scaler]"r"(scaler), [shift]"r"(shift)
       : "cc");
  return (Unsigned64{hi} << 32) | lo;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

IMPLEMENT inline
Unsigned64
Scaler_shift::transform(Unsigned64 value) const
{
  Mword dummy1, dummy2, dummy3;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umulh   %1, %0, %x[scaler]      \n\t"
       "mul     %0, %0, %x[scaler]      \n\t"
       "mov     %2, #32                 \n\t"
       "sub     %3, %2, %x[shift]       \n\t"
       "add     %2, %2, %x[shift]       \n\t"
       "lsr     %0, %0, %3              \n\t"
       "lsl     %1, %1, %2              \n\t"
       "orr     %0, %0, %1              \n\t"
       : "+r"(value), "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3)
       : [scaler]"r"(scaler), [shift]"r"(shift)
       : "cc");
  return value;
}
