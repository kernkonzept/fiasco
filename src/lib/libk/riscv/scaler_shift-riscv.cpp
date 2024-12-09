IMPLEMENTATION [riscv && 32bit]:

IMPLEMENT inline
Unsigned64
Scaler_shift::transform(Unsigned64 value) const
{
  Mword lo = value & 0xffffffff;
  Mword hi = value >> 32;
  Mword dummy1, dummy2, dummy3, dummy4;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm (/* compute (value * scaler) */
       "mulhu   %3, %0, %[scaler]       \n\t"
       "mul     %0, %0, %[scaler]       \n\t"  /* {%3,%0} contains (value.lo * scaler) */
       "mulhu   %4, %1, %[scaler]       \n\t"
       "mul     %1, %1, %[scaler]       \n\t"  /* {%4,%1} contains (value.hi * scaler) */
       "add     %1, %1, %3              \n\t"
       "sltu    %5, %1, %3              \n\t"  /* sltu + add is an add with carry from a1 + t0 */
       "add     %2, %4, %5              \n\t"  /* {%2,%1,%0} contains (timer * scaler) */
       /* compute {%2,%1,%0} >> (32 - shift) */
       "li      %3, 31                  \n\t"  /* RISC-V only supports 5-bit shifts. Given that 0 <= shift <= 31, */
       "sub     %3, %3, %[shift]        \n\t"  /* we can use a second constant shift-instruction as workaround. */
       "sll     %4, %1, %[shift]        \n\t"
       "sll     %5, %2, %[shift]        \n\t"
       "srli    %0, %0, 1               \n\t"
       "srl     %0, %0, %3              \n\t"
       "or      %0, %0, %4              \n\t"
       "srli    %1, %1, 1               \n\t"
       "srl     %1, %1, %3              \n\t"
       "or      %1, %1, %5              \n\t"
       : "+r"(lo), "+r"(hi),
         "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3), "=&r"(dummy4)
       : [scaler]"r"(Mword(scaler)), [shift]"r"(Mword(shift))
  );
  return (Unsigned64{hi} << 32) | lo;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [riscv && 64bit]:

IMPLEMENT inline
Unsigned64
Scaler_shift::transform(Unsigned64 value) const
{
  Mword dummy1, dummy2, dummy3;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("mulhu   %1, %0, %[scaler]      \n\t"
       "mul     %0, %0, %[scaler]      \n\t" /* {%1,%0} contains (timer * scaler) */
       "li      %2, 32                 \n\t"
       "subw    %3, %2, %[shift]       \n\t"
       "addw    %2, %2, %[shift]       \n\t"
       "srl     %0, %0, %3             \n\t" /* %0 = %0 >> (32 - shift) */
       "sll     %1, %1, %2             \n\t" /* %1 = %1 << (32 + shift) */
       "or      %0, %0, %1             \n\t" /* OR the "out-shifted" bits to %0 */
       : "+r"(value), "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3)
       // Explicit casts are necessary because otherwise the compiler
       // sign-extends the Unsigned32 values.
       : [scaler]"r"(Mword{scaler}), [shift]"r"(Mword{shift})
  );
  return value;
}
