INTERFACE [arm]:

#include "alternatives.h"

// preprocess off
#define ATOMIC_LX_SX_OP_(name, op, size, pfx)                                  \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  void                                                                         \
  _lx_sx_atomic_##name(T *mem, V value)                                        \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    asm volatile (                                                             \
        "     prfm  pstl1strm, %[mem] \n"                                      \
        "1:   ldxr  %"#pfx"[res], %[mem] \n"                                   \
        "   " #op " %"#pfx"[res], %"#pfx"[res], %"#pfx"[val] \n"               \
        "     stxr  %w[tmp], %"#pfx"[res], %[mem] \n"                          \
        "     cbnz  %w[tmp], 1b \n"                                            \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  _lx_sx_atomic_fetch_##name(T *mem, V value)                                  \
  {                                                                            \
    T val = value;                                                             \
    T res, old;                                                                \
    Mword tmp;                                                                 \
                                                                               \
    asm volatile (                                                             \
        "     prfm  pstl1strm, %[mem] \n"                                      \
        "1:   ldxr  %"#pfx"[old], %[mem] \n"                                   \
        "   " #op " %"#pfx"[res], %"#pfx"[old], %"#pfx"[val] \n"               \
        "     stxr  %w[tmp], %"#pfx"[res], %[mem] \n"                          \
        "     cbnz  %w[tmp], 1b \n"                                            \
        : [res] "=&r" (res), [old] "=&r" (old), [tmp] "=&r" (tmp),             \
          [mem] "+Q" (*mem)                                                    \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  _lx_sx_atomic_##name##_fetch(T *mem, V value)                                \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    asm volatile (                                                             \
        "     prfm  pstl1strm, %[mem] \n"                                      \
        "1:   ldxr  %"#pfx"[res], %[mem] \n"                                   \
        "   " #op " %"#pfx"[res], %"#pfx"[res], %"#pfx"[val] \n"               \
        "     stxr  %w[tmp], %"#pfx"[res], %[mem] \n"                          \
        "     cbnz  %w[tmp], 1b \n"                                            \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return res;                                                                \
  }
#define ATOMIC_LX_SX_OP(name, op)                                              \
  ATOMIC_LX_SX_OP_(name, op, 4, w)                                             \
  ATOMIC_LX_SX_OP_(name, op, 8, x)

ATOMIC_LX_SX_OP(and, and)
ATOMIC_LX_SX_OP(or, orr)
ATOMIC_LX_SX_OP(add, add)
#undef ATOMIC_LX_SX_OP_
#undef ATOMIC_LX_SX_OP


#define ATOMIC_LX_SX_EXCHANGE(size, pfx)                                       \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  _lx_sx_atomic_exchange(T *mem, V value)                                      \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    asm volatile (                                                             \
        "     prfm  pstl1strm, %[mem] \n"                                      \
        "1:   ldxr  %"#pfx"[res], %[mem] \n"                                   \
        "     stxr  %w[tmp], %"#pfx"[val], %[mem] \n"                          \
        "     cbnz  %w[tmp], 1b \n"                                            \
        : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)              \
        : [val] "r" (val)                                                      \
        : "cc");                                                               \
    return res;                                                                \
  }
ATOMIC_LX_SX_EXCHANGE(4, w)
ATOMIC_LX_SX_EXCHANGE(8, x)
#undef ATOMIC_LX_SX_EXCHANGE


#define ATOMIC_LSE_OP_(name, op, rop, neg, size, pfx)                          \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  void                                                                         \
  _lse_atomic_##name(T *mem, V value)                                          \
  {                                                                            \
    T val = value;                                                             \
                                                                               \
    asm volatile (                                                             \
        ".arch_extension lse \n"                                               \
        "st"#op " %"#pfx"[val], %[mem] \n"                                     \
        : [mem] "+Q" (*mem)                                                    \
        : [val] "r" (neg val));                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  _lse_atomic_fetch_##name(T *mem, V value)                                    \
  {                                                                            \
    T val = value;                                                             \
    T old;                                                                     \
                                                                               \
    asm volatile (                                                             \
        ".arch_extension lse \n"                                               \
        "ld"#op " %"#pfx"[val], %"#pfx"[old], %[mem] \n"                       \
        : [old] "=r" (old), [mem] "+Q" (*mem)                                  \
        : [val] "r" (neg val));                                                \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  _lse_atomic_##name##_fetch(T *mem, V value)                                  \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
                                                                               \
    asm volatile (                                                             \
        ".arch_extension lse \n"                                               \
        "ld"#op " %"#pfx"[val], %"#pfx"[res], %[mem] \n"                       \
        : [res] "=r" (res), [mem] "+Q" (*mem)                                  \
        : [val] "r" (neg val));                                                \
    return res rop val;                                                        \
  }
#define ATOMIC_LSE_OP(name, op, rop, neg)                                      \
  ATOMIC_LSE_OP_(name, op, rop, neg, 4, w)                                     \
  ATOMIC_LSE_OP_(name, op, rop, neg, 8, x)

// There is no ldand instruction, thus we have to negate the value and use ldclr.
ATOMIC_LSE_OP(and, clr, &, ~)
ATOMIC_LSE_OP(or, set, |, )
ATOMIC_LSE_OP(add, add, +, )
#undef ATOMIC_LSE_OP_
#undef ATOMIC_LSE_OP

#define ATOMIC_LSE_EXCHANGE(size, pfx)                                         \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  _lse_atomic_exchange(T *mem, V value)                                        \
  {                                                                            \
    T val = value;                                                             \
    T old;                                                                     \
                                                                               \
    asm volatile (                                                             \
        ".arch_extension lse \n"                                               \
        "swp %"#pfx"[val], %"#pfx"[old], %[mem] \n"                            \
        : [old] "=r" (old), [mem] "+Q" (*mem)                                  \
        : [val] "r" (val));                                                    \
    return old;                                                                \
  }
ATOMIC_LSE_EXCHANGE(4, w)
ATOMIC_LSE_EXCHANGE(8, x)
#undef ATOMIC_LSE_EXCHANGE

struct _boot_cpu_has_lse : public Alternative_static_functor<_boot_cpu_has_lse>
{
  static bool probe()
  {
    Mword isar0;
    asm ("mrs %0, ID_AA64ISAR0_EL1": "=r" (isar0));
    return ((isar0 >> 20) & 0xf) >= 0b10;
  }
};

#define ATOMIC_IMPL(ret, name)                                                 \
  template<typename T, typename V> inline                                      \
  ret                                                                          \
  name(T *mem, V value)                                                        \
  {                                                                            \
    if constexpr(TAG_ENABLED(arm_lse))                                         \
      return _lse_##name(mem, value);                                          \
    else if constexpr(TAG_ENABLED(arm_lse_may))                                \
      return _boot_cpu_has_lse() ? _lse_##name(mem, value)                     \
                                 : _lx_sx_##name(mem, value);                  \
    else                                                                       \
      return _lx_sx_##name(mem, value);                                        \
  }
ATOMIC_IMPL(void, atomic_and)
ATOMIC_IMPL(void, atomic_or)
ATOMIC_IMPL(void, atomic_add)
ATOMIC_IMPL(T, atomic_fetch_and)
ATOMIC_IMPL(T, atomic_fetch_or)
ATOMIC_IMPL(T, atomic_fetch_add)
ATOMIC_IMPL(T, atomic_and_fetch)
ATOMIC_IMPL(T, atomic_or_fetch)
ATOMIC_IMPL(T, atomic_add_fetch)
ATOMIC_IMPL(T, atomic_exchange)
#undef ATOMIC_IMPL

// preprocess on

//----------------------------------------------------------------------------
IMPLEMENTATION[arm]:

template< typename T > requires(sizeof(T) == 4) inline
T
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldr %w0, %1" : "=r" (res) : "m"(*p));
  return res;
}

template< typename T > requires(sizeof(T) == 8) inline
T
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldr %x0, %1" : "=r" (res) : "m"(*p));
  return res;
}

template< typename T, typename V > requires(sizeof(T) == 4) inline
void
atomic_store(T *p, V value)
{
  T val = value;
  asm volatile ("str %w1, %0" : "=m" (*p) : "r" (val));
}

template< typename T, typename V > requires(sizeof(T) == 8) inline
void
atomic_store(T *p, V value)
{
  T val = value;
  asm volatile ("str %x1, %0" : "=m" (*p) : "r" (val));
}

//----------------------------------------------------------------------------
IMPLEMENTATION[arm]:

inline
bool
_lx_sx_cas_arch(Mword *m, Mword o, Mword n)
{
  Mword tmp, res;

  asm volatile
    ("mov     %[res], #1          \n"
     "prfm    pstl1strm, %[m]     \n"
     "1:                          \n"
     "ldxr    %[tmp], %[m]        \n"
     "cmp     %[tmp], %[o]        \n"
     "b.ne    2f                  \n"
     "stxr    %w[res], %[n], %[m] \n"
     "cbnz    %w[res], 1b         \n"
     "dmb     ish                 \n"
     "2:                          \n"
     : [tmp] "=&r" (tmp), [res] "=&r" (res), [m] "+Q" (*m)
     : [n] "r" (n), [o] "r" (o)
     : "cc", "memory");

  // res == 0 is ok
  // res == 1 is failed

  return !res;
}

/// Compare-and-swap with acquire/release memory order.
inline
bool
_lse_cas_arch(Mword *mem, Mword oldval, Mword newval)
{
  Mword old = oldval;
  asm volatile (
      ".arch_extension lse \n"
      "casal %[o], %[n], %[mem] \n"
      : [o] "+r" (old), [mem] "+Q" (*mem)
      : [n] "r" (newval));

  return old == oldval;
}

inline
bool
cas_arch(Mword *mem, Mword oldval, Mword newval)
{
  if constexpr (TAG_ENABLED(arm_lse))
    return _lse_cas_arch(mem, oldval, newval);
  else if constexpr(TAG_ENABLED(arm_lse_may))
    return _boot_cpu_has_lse() ? _lse_cas_arch(mem, oldval, newval)
                               : _lx_sx_cas_arch(mem, oldval, newval);
  else
    return _lx_sx_cas_arch(mem, oldval, newval);
}
