INTERFACE [riscv]:

// preprocess off
#define ATOMIC_OP_(op, size, suffix)                                           \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  void                                                                         \
  atomic_##op(T *mem, V value)                                                 \
  {                                                                            \
    T val = value;                                                             \
    T prev;                                                                    \
                                                                               \
    __asm__ __volatile__ (                                                     \
      "amo" #op "." #suffix " %[prev], %[mask], %[mem]"                        \
      : [prev]"=r" (prev), [mem]"+A"(*mem)                                     \
      : [mask]"r" (val)                                                        \
      : "memory");                                                             \
  }
#define ATOMIC_OP(op)  \
  ATOMIC_OP_(op, 4, w) \
  ATOMIC_OP_(op, 8, d)

ATOMIC_OP(or)
ATOMIC_OP(and)
ATOMIC_OP(add)
#undef ATOMIC_OP_
#undef ATOMIC_OP


#define ATOMIC_FETCH_OP_(name, op, size, suffix)                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    T val = value;                                                             \
    T prev;                                                                    \
                                                                               \
    __asm__ __volatile__ (                                                     \
      "amo" #op "." #suffix " %[prev], %[mask], %[mem]"                        \
      : [prev]"=r" (prev), [mem]"+A"(*mem)                                     \
      : [mask]"r" (val)                                                        \
      : "memory");                                                             \
    return prev;                                                               \
  }

#define ATOMIC_FETCH_OP(name, op)  \
  ATOMIC_FETCH_OP_(name, op, 4, w) \
  ATOMIC_FETCH_OP_(name, op, 8, d)

ATOMIC_FETCH_OP(fetch_or, or)
ATOMIC_FETCH_OP(fetch_and, and)
ATOMIC_FETCH_OP(fetch_add, add)
ATOMIC_FETCH_OP(exchange, swap)
#undef ATOMIC_FETCH_OP_
#undef ATOMIC_FETCH_OP


#define ATOMIC_OP_FETCH_(op, size, suffix)                                     \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  atomic_##op##_fetch(T *mem, V value)                                         \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
                                                                               \
    __asm__ __volatile__ (                                                     \
      "amo" #op "." #suffix " %[res], %[val], %[mem] \n"                       \
      #op "         %[res], %[res], %[val] \n"                                 \
      : [res]"=&r" (res), [mem]"+A"(*mem)                                      \
      : [val]"r" (val)                                                         \
      : "memory");                                                             \
    return res;                                                                \
  }

#define ATOMIC_OP_FETCH(op)  \
  ATOMIC_OP_FETCH_(op, 4, w) \
  ATOMIC_OP_FETCH_(op, 8, d)

ATOMIC_OP_FETCH(or)
ATOMIC_OP_FETCH(and)
ATOMIC_OP_FETCH(add)
#undef ATOMIC_OP_FETCH_
#undef ATOMIC_OP_FETCH
// preprocess on

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "asm_riscv.h"

template<typename T> ALWAYS_INLINE inline
T
atomic_load(T const *mem)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
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
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
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
void
local_atomic_and(Mword *mem, Mword mask)
{
  atomic_and(mem, mask);
}

inline
void
local_atomic_or(Mword *mem, Mword bits)
{
  atomic_or(mem, bits);
}

inline
void
local_atomic_add(Mword *mem, Mword value)
{
  atomic_add(mem, value);
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
