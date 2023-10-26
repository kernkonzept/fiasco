#pragma once

namespace Asm_access {

#define READ(type, inst) \
  inline type read(type const *mem) \
  { \
    type val; \
    asm volatile (inst " %[val], %[mem]" \
                  : [val] "=r" (val) : [mem] "m" (*mem)); \
    return val; \
  }

#define WRITE(type, inst) \
  inline void write(type val, type *mem) \
  { \
    asm volatile (inst " %[val], %[mem]" \
                  : [mem] "=m" (*mem) : [val] "r" (val)); \
  }

READ(Unsigned8,  "ldrb");
READ(Unsigned16, "ldrh");
READ(Unsigned32, "ldr");
READ(Mword,      "ldr");

inline
Unsigned64
read_non_atomic(Unsigned64 const *mem)
{
  Unsigned32 lo, hi;
  Unsigned32 const *mem32 = reinterpret_cast<Unsigned32 const *>(mem);

  asm volatile ("ldr %[val], %[mem]" : [val] "=r" (lo) : [mem] "m" (mem32[0]));
  asm volatile ("ldr %[val], %[mem]" : [val] "=r" (hi) : [mem] "m" (mem32[1]));

  return ((Unsigned64)hi << 32) | lo;
}

WRITE(Unsigned8,  "strb");
WRITE(Unsigned16, "strh");
WRITE(Unsigned32, "str");
WRITE(Mword,      "str");

inline
void
write_non_atomic(Unsigned64 val, Unsigned64 *mem)
{
  Unsigned32 *mem32 = reinterpret_cast<Unsigned32 *>(mem);
  Unsigned32 lo = val & 0xffffffff;
  Unsigned32 hi = val >> 32;

  asm volatile ("str %[val], %[mem]" : [mem] "=m" (mem32[0]) : [val] "r" (lo));
  asm volatile ("str %[val], %[mem]" : [mem] "=m" (mem32[1]) : [val] "r" (hi));
}

#undef READ
#undef WRITE

}
