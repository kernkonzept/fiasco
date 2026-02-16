INTERFACE[arm && arm_v6plus]:

#include "mem.h"

// preprocess off
#define ATOMIC_OP(name, op)                                                    \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 4) inline                                              \
  void                                                                         \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
    Mem::prefetch_w(mem);                                                      \
    asm volatile (                                                             \
        "1:   ldrex %[res], %[mem] \n"                                         \
              #op " %[res], %[res], %[val] \n"                                 \
        "     strex %[tmp], %[res], %[mem] \n"                                 \
        "     teq   %[tmp], #0 \n"                                             \
        "     bne   1b "                                                       \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 4) inline                                              \
  T                                                                            \
  atomic_fetch_##name(T *mem, V value)                                         \
  {                                                                            \
    T val = value;                                                             \
    T res, old;                                                                \
    Mword tmp;                                                                 \
    Mem::prefetch_w(mem);                                                      \
    asm volatile (                                                             \
        "1:   ldrex %[old], %[mem] \n"                                         \
              #op " %[res], %[old], %[val] \n"                                 \
        "     strex %[tmp], %[res], %[mem] \n"                                 \
        "     teq   %[tmp], #0 \n"                                             \
        "     bne   1b "                                                       \
        : [res] "=&r" (res), [old] "=&r" (old), [tmp] "=&r" (tmp),             \
          [mem] "+Q" (*mem)                                                    \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 4) inline                                              \
  T                                                                            \
  atomic_##name##_fetch(T *mem, V value)                                       \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
    Mem::prefetch_w(mem);                                                      \
    asm volatile (                                                             \
        "1:   ldrex %[res], %[mem] \n"                                         \
              #op " %[res], %[res], %[val] \n"                                 \
        "     strex %[tmp], %[res], %[mem] \n"                                 \
        "     teq   %[tmp], #0 \n"                                             \
        "     bne   1b "                                                       \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return res;                                                                \
  }
ATOMIC_OP(and, and)
ATOMIC_OP(or, orr)
ATOMIC_OP(add, add)
#undef ATOMIC_OP
// preprocess on

// --------------------------------------------------------------------
INTERFACE[arm && (arm_v7plus || (arm_v6 && mp))]:

// preprocess off
#define ATOMIC_OP(name, opl, oph)                                              \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 8) inline                                              \
  void                                                                         \
  atomic_##name(T *mem, V value)                                               \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
    Mem::prefetch_w(mem);                                                      \
    asm volatile (                                                             \
        "1:   ldrexd %[res], %H[res], %[mem] \n"                               \
              #opl " %[res], %[res], %[val] \n"                                \
              #oph " %H[res], %H[res], %H[val] \n"                             \
        "     strexd %[tmp], %[res], %H[res], %[mem] \n"                       \
        "     teq    %[tmp], #0 \n"                                            \
        "     bne    1b "                                                      \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 8) inline                                              \
  T                                                                            \
  atomic_fetch_##name(T *mem, V value)                                         \
  {                                                                            \
    T val = value;                                                             \
    T res, old;                                                                \
    Mword tmp;                                                                 \
    Mem::prefetch_w(mem);                                                      \
    asm (                                                                      \
        "1:   ldrexd %[old], %H[res], %[mem] \n"                               \
              #opl " %[res], %[old], %[val] \n"                                \
              #oph " %H[res], %H[old], %H[val] \n"                             \
        "     strexd %[tmp], %[res], %H[res], %[mem] \n"                       \
        "     teq    %[tmp], #0 \n"                                            \
        "     bne    1b "                                                      \
        : [res] "=&r" (res), [old] "=&r" (old), [tmp] "=&r" (tmp),             \
          [mem] "+Q" (*mem)                                                    \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == 8) inline                                              \
  T                                                                            \
  atomic_##name##_fetch(T *mem, V value)                                       \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
    Mem::prefetch_w(mem);                                                      \
    asm volatile (                                                             \
        "1:   ldrexd %[res], %H[res], %[mem] \n"                               \
              #opl " %[res], %[res], %[val] \n"                                \
              #oph " %H[res], %H[res], %H[val] \n"                             \
        "     strexd %[tmp], %[res], %H[res], %[mem] \n"                       \
        "     teq    %[tmp], #0 \n"                                            \
        "     bne    1b "                                                      \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return res;                                                                \
  }
ATOMIC_OP(and, and, and)
ATOMIC_OP(or, orr, orr)
ATOMIC_OP(add, adds, adc)
#undef ATOMIC_OP
// preprocess on

//----------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus]:

template<typename T, typename V>
requires(sizeof(T) == 4) inline
T
atomic_exchange(T *mem, V value)
{
  T val = value;
  T res;
  Mword tmp;

  Mem::prefetch_w(mem);
  asm volatile (
      "1:   ldrex %[res], [%[mem]] \n"
      "     strex %[tmp], %[val], [%[mem]] \n"
      "     cmp   %[tmp], #0 \n"
      "     bne   1b "
      : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
      : [mem] "r" (mem), [val] "r" (val)
      : "cc");
  return res;
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && (arm_v7plus || (arm_v6 && mp))]:

template<typename T, typename V>
requires(sizeof(T) == 8) inline
T
atomic_exchange(T *mem, V value)
{
  T val = value;
  T res;
  Mword tmp;
  Mem::prefetch_w(mem);
  asm volatile (
      "1:   ldrexd %[res], %H[res], %[mem] \n"
      "     strexd %[tmp], %[val], %H[val], %[mem] \n"
      "     cmp    %[tmp], #0 \n"
      "     bne    1b "
      : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)
      : [val] "r" (val)
      : "cc");
  return res;
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6 && !mp]:

#include "processor.h"

template<typename T, typename V>
requires(sizeof(T) == 8) inline NEEDS ["processor.h"]
T
atomic_exchange(T *mem, V value)
{
  Mword s = Proc::cli_save();
  T val = value;
  T res;
  Mem::prefetch_w(mem);
  asm volatile (
      "1:   ldrd %[res], %H[res], %[mem] \n"
      "     strd %[val], %H[val], %[mem]   "
      : [res] "=&r" (res), [mem] "+m" (*mem)
      : [val] "r" (val));
  Proc::sti_restore(s);
  return res;
}

template<typename T, typename V>
requires(sizeof(T) == 8) inline NEEDS ["processor.h"]
T
atomic_add_fetch(T *mem, V value)
{
  Mword s = Proc::cli_save();
  T val = value;
  T res;
  Mem::prefetch_w(mem);
  asm volatile (
      "1:   ldrd   %[res], %H[res], %[mem] \n"
      "     adds   %[res], %[res], %[val] \n"
      "     adc    %H[res], %H[res], %H[val] \n"
      "     strd   %[res], %H[res], %[mem]"
      : [res] "=&r" (res), [mem] "+m" (*mem)
      : [val] "r" (val)
      : "cc");
  Proc::sti_restore(s);
  return res;
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm]:

template<typename T>
requires(sizeof(T) == 4) inline
T
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldr %0, %1" : "=&r" (res) : "m"(*p));
  return res;
}

template<typename T, typename V>
requires(sizeof(T) == 4) inline
void
atomic_store(T *p, V value)
{
  T val = value;
  asm volatile("str %1, %0" : "=m"(*p) : "r"(val));
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && (!mp || arm_lpae || arm_v8plus)]:

template<typename T>
requires(sizeof(T) == 8) inline
T
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldrd %0, %H0, %1" : "=&r" (res) : "m" (*p));
  return res;
}

template<typename T, typename V>
requires(sizeof(T) == 8) inline
void
atomic_store(T *p, V value)
{
  T val = value;
  asm volatile ("strd %1, %H1, %0" : "=m" (*p) : "r" (val));
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && mp && !arm_lpae && !arm_v8plus]:

template<typename T>
requires(sizeof(T) == 8) inline
T
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldrexd %0, %H0, [%1]" : "=&r" (res) : "r" (p), "Qo" (*p));
  return res;
}

template<typename T, typename V>
requires(sizeof(T) == 8) inline
void
atomic_store(T *p, V value)
{
  T val = value;
  long long tmp;
  Mem::prefetch_w(p);
  asm volatile (
      "1: ldrexd %0, %H0, [%2] \n"
      "   strexd %0, %3, %H3, [%2] \n"
      "   teq    %0, #0 \n"
      "   bne    1b"
      : "=&r"(tmp), "=Qo"(*p)
      : "r"(p), "r"(val)
      : "cc");
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus]:

#include "mem.h"

inline NEEDS["mem.h"]
bool
cas_arch(Mword *m, Mword o, Mword n)
{
  Mword tmp, res;

  asm volatile
    ("mov     %[res], #1           \n"
     "1:                           \n"
     "ldr     %[tmp], [%[m]]       \n"
     "teq     %[tmp], %[o]         \n"
     "bne     2f                   \n"
     "ldrex   %[tmp], [%[m]]       \n"
     "teq     %[tmp], %[o]         \n"
     "strexeq %[res], %[n], [%[m]] \n"
     "teq     %[res], #1           \n"
     "beq     1b                   \n"
     "2:                           \n"
     : [tmp] "=&r" (tmp), [res] "=&r" (res), "+m" (*m)
     : [n] "r" (n), [m] "r" (m), [o] "r" (o)
     : "cc", "memory");
  Mem::dmb();

  // res == 0 is ok
  // res == 1 is failed

  return !res;
}
