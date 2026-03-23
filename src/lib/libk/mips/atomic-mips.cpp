INTERFACE [mips]:

#include "asm_mips.h"

// preprocess off
#define ATOMIC_OP(name, op)                                                    \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  void                                                                         \
  __f_atomic_##name(T *mem, V value)                                           \
  {                                                                            \
    T val = value;                                                             \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            ASM_LL " %[tmp], %[mem]  \n"                                       \
            op    " %[tmp], %[val] \n"                                         \
            ASM_SC " %[tmp], %[mem]  \n"                                       \
            : [tmp] "=&r" (tmp), [mem] "+ZC" (*mem)                            \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  T                                                                            \
  __f_atomic_fetch_##name(T *mem, V value)                                     \
  {                                                                            \
    T val = value;                                                             \
    T old;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            ASM_LL   " %[tmp], %[ptr]  \n"                                     \
            "move      %[old], %[tmp]  \n"                                     \
            op      " %[tmp], %[val]    \n"                                    \
            ASM_SC   " %[tmp], %[ptr]  \n"                                     \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [old] "=&r"(old)          \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  T                                                                            \
  __f_atomic_##name##_fetch(T *mem, V value)                                   \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            ASM_LL   " %[tmp], %[ptr]  \n"                                     \
            op      " %[tmp], %[val]    \n"                                    \
            "move      %[res], %[tmp]  \n"                                     \
            ASM_SC   " %[tmp], %[ptr]  \n"                                     \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [res] "=&r"(res)          \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
    return res;                                                                \
  }
ATOMIC_OP(or, "or")
ATOMIC_OP(and, "and")
ATOMIC_OP(add, ASM_ADDU)
#undef ATOMIC_OP

template<typename T, typename V>
requires(sizeof(T) == sizeof(Mword)) inline
T
__f_atomic_exchange(T *mem, V value)
{
  T val = value;
  T old;
  Mword tmp;

  do
    {
      __asm__ __volatile__(
          ASM_LL   " %[old], %[ptr]  \n"
          "move      %[tmp], %[val]  \n"
          ASM_SC   " %[tmp], %[ptr]  \n"
          : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [old] "=&r"(old)
          : [val] "r" (val));
    }
  while (!tmp);
  return old;
}
// preprocess on

//----------------------------------------------------------------------------
INTERFACE [mips && mp]:

#include "mem.h"

// preprocess off
#define ATOMIC_OP_(name)                                                       \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  void                                                                         \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    Mem::mp_mb();                                                              \
    __f_atomic_##name<T, V>(mem, value);                                       \
    Mem::mp_mb();                                                              \
  }                                                                            \

#define ATOMIC_RET_OP_(name)                                                   \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  T                                                                            \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    Mem::mp_mb();                                                              \
    T res = __f_atomic_##name<T, V>(mem, value);                           \
    Mem::mp_mb();                                                              \
    return res;                                                                \
  }

#define ATOMIC_OP(name)                                                        \
  ATOMIC_OP_(name)                                                             \
  ATOMIC_RET_OP_(fetch_##name)                                                 \
  ATOMIC_RET_OP_(name##_fetch)

ATOMIC_OP(or)
ATOMIC_OP(and)
ATOMIC_OP(add)
#undef ATOMIC_OP_
#undef ATOMIC_RET_OP_
#undef ATOMIC_OP

template<typename T, typename V>
requires(sizeof(T) == sizeof(Mword)) inline
T
atomic_exchange(T *mem, V value)
{
  Mem::mp_mb();
  T res = __f_atomic_exchange<T, V>(mem, value);
  Mem::mp_mb();
  return res;
}
// preprocess on

//----------------------------------------------------------------------------
INTERFACE [mips && !mp]:

#include "mem.h"

// preprocess off
#define ATOMIC_OP_(name)                                                       \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  void                                                                         \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    __f_atomic_##name<T, V>(mem, value);                                       \
  }                                                                            \

#define ATOMIC_RET_OP_(name)                                                   \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == sizeof(Mword)) inline                                  \
  T                                                                            \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    return __f_atomic_##name<T, V>(mem, value);                                \
  }

#define ATOMIC_OP(name)                                                        \
  ATOMIC_OP_(name)                                                             \
  ATOMIC_RET_OP_(fetch_##name)                                                 \
  ATOMIC_RET_OP_(name##_fetch)                                                 \

ATOMIC_OP(or)
ATOMIC_OP(and)
ATOMIC_OP(add)
#undef ATOMIC_OP_
#undef ATOMIC_RET_OP_
#undef ATOMIC_OP

template<typename T, typename V>
requires(sizeof(T) == sizeof(Mword)) inline
T
atomic_exchange(T *mem, V value)
{
  return __f_atomic_exchange<T, V>(mem, value);
}
// preprocess on

//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

template<typename T> requires(sizeof(T) == 4) inline
T
atomic_load(T const *mem)
{
  T res;
  __asm__ __volatile__ ("lw %0, %1" : "=r" (res) : "m"(*mem));
  return res;
}

template<typename T> requires(sizeof(T) == 8) inline
T
atomic_load(T const *mem)
{
  T res;
  __asm__ __volatile__ ("ld %0, %1" : "=r" (res) : "m"(*mem));
  return res;
}

template<typename T, typename V> requires(sizeof(T) == 4) inline
void
atomic_store(T *mem, V value)
{
  T val = value;
  __asm__ __volatile__ ("sw %1, %0" : "=m" (*mem) : "r" (val));
}

template<typename T, typename V> requires(sizeof(T) == 8) inline
void
atomic_store(T *mem, V value)
{
  T val = value;
  __asm__ __volatile__ ("sd %1, %0" : "=m" (*mem) : "r" (val));
}

inline
void
local_atomic_and(Mword *mem, Mword mask)
{
  __f_atomic_and<Mword, Mword>(mem, mask);
}

inline
void
local_atomic_or(Mword *mem, Mword bits)
{
  __f_atomic_or<Mword, Mword>(mem, bits);
}

inline
void
local_atomic_add(Mword *mem, Mword value)
{
  __f_atomic_add<Mword, Mword>(mem, value);
}

// ``unsafe'' stands for no safety according to the size of the given type.
// There are type safe versions of the cas operations in the architecture
// independent part of atomic that use the unsafe versions and make a type
// check.

inline NEEDS["asm_mips.h"]
bool
local_cas_unsafe(Mword *ptr, Mword oldval, Mword newval)
{
  Mword ret;

  __asm__ __volatile__(
      "     .set    push                \n"
      "     .set    noat    #CAS        \n"
      "     .set    noreorder           \n"
      "1:   " ASM_LL " %[ret], %[ptr]   \n"
      "     bne     %[ret], %z[old], 2f \n"
      "       move $1, %z[newval]       \n"
      "     " ASM_SC " $1, %[ptr]       \n"
      "     beqz    $1, 1b              \n"
      "       nop                       \n"
      "2:                               \n"
      "     .set    pop                 \n"
      : [ret] "=&r" (ret), [ptr] "+ZC" (*ptr)
      : [old] "Jr" (oldval), [newval] "Jr" (newval)
      : "memory"); // for some unknown reason this is necessary for newer
                   // gcc compilers

  // true is ok
  // false is failed
  return ret == oldval;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mips && mp]:

inline
bool
cas_arch(Mword *m, Mword o, Mword n)
{
  Mem::mp_mb();
  Mword ret = local_cas_unsafe(m, o, n);
  Mem::mp_mb();
  return ret;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [mips && !mp]:

inline
bool
cas_arch(Mword *m, Mword o, Mword n)
{ return local_cas_unsafe(m, o, n); }

