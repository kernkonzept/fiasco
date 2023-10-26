#pragma once

namespace Asm_access {

#define READ(type, inst, mod) \
  inline type read(type const *mem) \
  { \
    type val; \
    asm volatile (inst " " mod"[val], %[mem]" \
                  : [val] "=r" (val) : [mem] "m" (*mem)); \
    return val; \
  }

#define WRITE(type, inst, mod) \
  inline void write(type val, type *mem) \
  { \
    asm volatile (inst " " mod"[val], %[mem]" \
                  : [mem] "=m" (*mem) : [val] "r" (val)); \
  }

READ(Unsigned8,  "ldrb", "%w");
READ(Unsigned16, "ldrh", "%w");
READ(Unsigned32, "ldr",  "%w");
READ(Unsigned64, "ldr",  "%");
READ(Mword,      "ldr",  "%");

WRITE(Unsigned8,  "strb", "%w");
WRITE(Unsigned16, "strh", "%w");
WRITE(Unsigned32, "str",  "%w");
WRITE(Unsigned64, "str",  "%");
WRITE(Mword,      "str",  "%");

#undef READ
#undef WRITE

inline Unsigned64 read_non_atomic(Unsigned64 const *mem)
{ return read(mem); }

inline void write_non_atomic(Unsigned64 val, Unsigned64 *mem)
{ return write(val, mem); }

}
