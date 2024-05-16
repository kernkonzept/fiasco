IMPLEMENTATION [riscv]:

#include "asm_riscv.h"

inline NEEDS["asm_riscv.h"]
Mword
atomic_and(Mword *l, Mword mask)
{
  Mword prev;

  __asm__ __volatile__ (
    "amoand." SZMOD " %[prev], %[mask], %[l]"
    : [prev]"=r" (prev), [l]"+A"(*l)
    : [mask]"r" (mask)
    : "memory");

  return prev;
}

inline NEEDS["asm_riscv.h"]
Mword
atomic_or(Mword *l, Mword bits)
{
  Mword prev;

  __asm__ __volatile__ (
    "amoor." SZMOD " %[prev], %[bits], %[l]"
    : [prev]"=r" (prev), [l]"+A"(*l)
    : [bits]"r" (bits)
    : "memory");

  return prev;
}

inline NEEDS["asm_riscv.h"]
Mword
atomic_add(Mword *l, Mword value)
{
  Mword prev;

  __asm__ __volatile__ (
    "amoadd." SZMOD " %[prev], %[value], %[l]"
    : [prev]"=r" (prev), [l]"+A"(*l)
    : [value]"r" (value)
    : "memory");

  return prev;
}


inline NEEDS["asm_riscv.h"]
Mword
atomic_xchg(Mword *l, Mword value)
{
  Mword prev;

  __asm__ __volatile__ (
    "amoswap." SZMOD " %[prev], %[value], %[l]"
    : [prev]"=r" (prev), [l]"+A"(*l)
    : [value]"r" (value)
    : "memory");

  return prev;
}

template<typename T, typename V> inline
T
atomic_exchange(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "invalid size of operand (must be 4 or 8 byte)");
  T val = value;
  T res;

  switch (sizeof(T))
    {
    case 4:
      __asm__ __volatile__ (
        "amoswap.w %[res], %[val], %[mem]"
        : [res]"=r" (res), [mem]"+A"(*mem)
        : [val]"r" (val)
        : "memory");
      return res;

    case 8:
      __asm__ __volatile__ (
        "amoswap.d %[res], %[val], %[mem]"
        : [res]"=r" (res), [mem]"+A"(*mem)
        : [val]"r" (val)
        : "memory");
      return res;
    }
}

template<typename T, typename V> inline
T
atomic_add_fetch(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "invalid size of operand (must be 4 or 8 byte)");
  T val = value;
  T res;

  switch (sizeof(T))
    {
    case 4:
      __asm__ __volatile__ (
        "amoadd.w %[res], %[val], %[mem]"
        : [res]"=r" (res), [mem]"+A"(*mem)
        : [val]"r" (val)
        : "memory");
      return res;

    case 8:
      __asm__ __volatile__ (
        "amoadd.d %[res], %[val], %[mem]"
        : [res]"=r" (res), [mem]"+A"(*mem)
        : [val]"r" (val)
        : "memory");
      return res;
    }
}

template<typename T> ALWAYS_INLINE inline
T
atomic_load(T const *mem)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_load supported for 4 and 8 byte types only");
  T res;
  switch (sizeof(T))
    {
    case 4:
      __asm__ __volatile__ ("lw %0, %1" : "=r" (res) : "m"(*mem));
      return res;

    case 8:
      __asm__ __volatile__ ("ld %0, %1" : "=r" (res) : "m"(*mem));
      return res;
    }
}

template<typename T, typename V> ALWAYS_INLINE inline
void
atomic_store(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_store supported for 4 and 8 byte types only");
  T val = value;
  switch (sizeof(T))
    {
    case 4:
      __asm__ __volatile__ ("sw %1, %0" : "=m" (*mem) : "r" (val));
      break;

    case 8:
      __asm__ __volatile__ ("sd %1, %0" : "=m" (*mem) : "r" (val));
      break;
    }
}

inline
Mword
local_atomic_and(Mword *l, Mword mask)
{
  return atomic_and(l, mask);
}

inline
Mword
local_atomic_or(Mword *l, Mword bits)
{
  return atomic_or(l, bits);
}

inline
Mword
local_atomic_add(Mword *l, Mword value)
{
  return atomic_add(l, value);
}

inline
Mword
local_atomic_xchg(Mword *l, Mword value)
{
  return atomic_xchg(l, value);
}

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline NEEDS["asm_riscv.h"]
bool
local_cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword prev;
  // Holds return value of SC instruction: 0 if successful, !0 otherwise
  Mword ret = 1;

  __asm__ __volatile__ (
    "0:                               \n"
    REG_LR " %[prev], %[ptr]          \n"
    "bne     %[prev], %[old], 1f      \n"
    REG_SC " %[ret],  %[newv], %[ptr] \n"
    "bnez    %[ret],  0b              \n"
    "1:                               \n"
    : [prev]"=&r" (prev), [ret]"+&r" (ret), [ptr]"+A"(*ptr)
    : [newv]"r" (newval), [old]"r" (oldval)
    : "memory");

  return ret == 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

inline NEEDS["asm_riscv.h"]
bool
cas_arch(Mword *ptr, Mword oldval, Mword newval)
{
  Mword prev;
  // Holds return value of SC instruction: 0 if successful, !0 otherwise
  Mword ret = 1;

  /*
    Other harts shall observe LR-SC retries in the same order they were
    executed. Therefore we need release semantics on SC here.

    An SC instruction can never be observed by another RISC-V hart before the
    immediately preceding LR. Therefore no acquire semantics on LR are needed.
  */
  __asm__ __volatile__ (
    "0:                                  \n"
    REG_LR "    %[prev], %[ptr]          \n"
    "bne        %[prev], %[old], 1f      \n"
    REG_SC ".rl %[ret],  %[newv], %[ptr] \n"
    "bnez       %[ret],  0b              \n"
    "fence      rw, rw                   \n"
    "1:                                  \n"
    : [prev]"=&r" (prev), [ret]"+&r" (ret), [ptr]"+A"(*ptr)
    : [newv]"r" (newval), [old]"r" (oldval)
    : "memory");

  return ret == 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv && !mp]:

inline
bool
cas_arch(Mword *m, Mword o, Mword n)
{
  return local_cas_unsafe(m, o, n);
}
