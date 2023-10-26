#pragma once

namespace Asm_access {

#define READ(type, inst, reg) \
  inline type read(type const *mem) \
  { \
    type val; \
    asm volatile (inst " %[mem], %[val]" \
                  : [val] reg (val) : [mem] "m" (*mem)); \
    return val; \
  }

#define WRITE(type, inst, reg) \
  inline void write(type val, type *mem) \
  { \
    asm volatile (inst " %[val], %[mem]" \
                  : [mem] "=m" (*mem) : [val] reg (val)); \
  }

READ(Unsigned8,  "movb", "=q");
READ(Unsigned16, "movw", "=r");
READ(Unsigned32, "movl", "=r");
READ(Unsigned64, "movq", "=r");
READ(Mword,      "movq", "=r");

WRITE(Unsigned8,  "movb", "qi");
WRITE(Unsigned16, "movw", "ri");
WRITE(Unsigned32, "movl", "ri");
WRITE(Unsigned64, "movq", "r");
WRITE(Mword,      "movq", "r");

#undef READ
#undef WRITE

}
