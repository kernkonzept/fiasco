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

READ(Unsigned8,   "lb");
READ(Unsigned16,  "lh");
READ(Unsigned32,  "lw");

WRITE(Unsigned8,  "sb");
WRITE(Unsigned16, "sh");
WRITE(Unsigned32, "sw");

#if __riscv_xlen == 32
  READ(Mword,       "lw");

  inline
  Unsigned64
  read_non_atomic(Unsigned64 const *mem)
  {
    Unsigned32 const *mem32 = reinterpret_cast<Unsigned32 const *>(mem);

    Unsigned32 lo = read(&mem32[0]);
    Unsigned32 hi = read(&mem32[1]);
    return (static_cast<Unsigned64>(hi) << 32) | lo;
  }

  WRITE(Mword,      "sw");

  inline
  void
  write_non_atomic(Unsigned64 val, Unsigned64 *mem)
  {
    Unsigned32 *mem32 = reinterpret_cast<Unsigned32 *>(mem);
    Unsigned32 lo = val & 0xffffffff;
    Unsigned32 hi = val >> 32;

    write(lo, &mem32[0]);
    write(hi, &mem32[1]);
  }
#elif __riscv_xlen == 64
  READ(Unsigned64,  "ld");
  READ(Mword,       "ld");

  inline Unsigned64 read_non_atomic(Unsigned64 const *mem)
  { return read(mem); }

  WRITE(Unsigned64,  "sd");
  WRITE(Mword,       "sd");

  inline void write_non_atomic(Unsigned64 val, Unsigned64 *mem)
  { return write(val, mem); }
#else
  #error __riscv_xlen must be either 32 or 64
#endif

#undef READ
#undef WRITE

}
