INTERFACE [mips]:

#include "asm_mips.h"
#include "mem.h"

// preprocess off
#define ATOMIC_OP_(name, op, size, suffix)                                     \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  void                                                                         \
  atomic_##name##_relaxed(T *mem, V value)                                     \
  {                                                                            \
    static_assert(cxx::is_integral_v<T>);                                      \
    T val = value;                                                             \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            "ll"#suffix " %[tmp], %[mem]  \n"                                  \
            op    " %[tmp], %[val] \n"                                         \
            "sc"#suffix " %[tmp], %[mem]  \n"                                  \
            : [tmp] "=&r" (tmp), [mem] "+ZC" (*mem)                            \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_fetch_##name##_relaxed(T *mem, V value)                               \
  {                                                                            \
    static_assert(cxx::is_integral_v<T>);                                      \
    T val = value;                                                             \
    T old;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            "ll"#suffix " %[tmp], %[ptr]  \n"                                  \
            "move      %[old], %[tmp]  \n"                                     \
            op      " %[tmp], %[val]    \n"                                    \
            "sc"#suffix " %[tmp], %[ptr]  \n"                                  \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [old] "=&r"(old)          \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
    return old;                                                                \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_##name##_fetch_relaxed(T *mem, V value)                               \
  {                                                                            \
    static_assert(cxx::is_integral_v<T>);                                      \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            "ll"#suffix " %[tmp], %[ptr]  \n"                                  \
            op      " %[tmp], %[val]    \n"                                    \
            "move      %[res], %[tmp]  \n"                                     \
            "sc"#suffix " %[tmp], %[ptr]  \n"                                  \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [res] "=&r"(res)          \
            : [val] "Ir" (val));                                               \
      }                                                                        \
    while (!tmp);                                                              \
    return res;                                                                \
  }

#define ATOMIC_OP(name, op)                                                    \
  ATOMIC_OP_(name, op, 4,  )                                                   \
  ATOMIC_OP_(name, op, 8, d)

ATOMIC_OP(or, "or")
ATOMIC_OP(and, "and")
ATOMIC_OP_(add, "addu", 4,  )
ATOMIC_OP_(add, "daddu", 8, d)
#undef ATOMIC_OP_
#undef ATOMIC_OP
// preprocess on

template<typename T> requires(sizeof(T) == 4) inline
T
atomic_load_relaxed(T const *mem)
{
  T res;
  __asm__ __volatile__ ("lw %0, %1" : "=r" (res) : "m"(*mem));
  return res;
}

template<typename T>
requires(sizeof(T) == 8 && sizeof(Mword) == 8) inline
T
atomic_load_relaxed(T const *mem)
{
  T res;
  __asm__ __volatile__ ("ld %0, %1" : "=r" (res) : "m"(*mem));
  return res;
}

template<typename T, typename V> requires(sizeof(T) == 4) inline
void
atomic_store_relaxed(T *mem, V value)
{
  T val = value;
  __asm__ __volatile__ ("sw %1, %0" : "=m" (*mem) : "r" (val));
}

template<typename T, typename V>
requires(sizeof(T) == 8 && sizeof(Mword) == 8) inline
void
atomic_store_relaxed(T *mem, V value)
{
  T val = value;
  __asm__ __volatile__ ("sd %1, %0" : "=m" (*mem) : "r" (val));
}

template<typename T> inline
T
atomic_load_acquire(T const *mem)
{
  T res = atomic_load_relaxed(mem);
  Mem::mp_mb();
  return res;
}

template<typename T> inline
T
atomic_load_seq_cst(T const *mem)
{
  Mem::mp_mb();
  T res = atomic_load_relaxed(mem);
  Mem::mp_mb();
  return res;
}

template<typename T, typename V> inline
void
atomic_store_release(T *mem, V value)
{
  Mem::mp_mb();
  atomic_store_relaxed(mem, value);
}

template<typename T, typename V> inline
void
atomic_store_seq_cst(T *mem, V value)
{
  Mem::mp_mb();
  atomic_store_relaxed(mem, value);
  Mem::mp_mb();
}

// preprocess off
#define ATOMIC_EXCHANGE(size, suffix)                                          \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_exchange_relaxed(T *mem, V value)                                     \
  {                                                                            \
    T val = value;                                                             \
    T old;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    do                                                                         \
      {                                                                        \
        __asm__ __volatile__(                                                  \
            "ll"#suffix " %[old], %[ptr]  \n"                                  \
            "move      %[tmp], %[val]     \n"                                  \
            "sc"#suffix " %[tmp], %[ptr]  \n"                                  \
            : [tmp] "=&r" (tmp), [ptr] "+ZC" (*mem), [old] "=&r"(old)          \
            : [val] "r" (val));                                                \
      }                                                                        \
    while (!tmp);                                                              \
    return old;                                                                \
  }
ATOMIC_EXCHANGE(4,  )
ATOMIC_EXCHANGE(8, d)
#undef ATOMIC_EXCHANGE

#define ATOMIC_CAS(size, suffix)                                               \
  template<typename T>                                                         \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  bool                                                                         \
  cas_relaxed(T *mem, T oldval, T newval)                                      \
  {                                                                            \
    T ret;                                                                     \
                                                                               \
    __asm__ __volatile__(                                                      \
        "     .set    push                \n"                                  \
        "     .set    noat    #CAS        \n"                                  \
        "     .set    noreorder           \n"                                  \
        "1:   ll"#suffix " %[ret], %[mem] \n"                                  \
        "     bne     %[ret], %z[old], 2f \n"                                  \
        "       move $1, %z[newval]       \n"                                  \
        "     sc"#suffix " $1, %[mem]     \n"                                  \
        "     beqz    $1, 1b              \n"                                  \
        "       nop                       \n"                                  \
        "2:                               \n"                                  \
        "     .set    pop                 \n"                                  \
        : [ret] "=&r" (ret), [mem] "+ZC" (*mem)                                \
        : [old] "Jr" (oldval), [newval] "Jr" (newval));                        \
                                                                               \
    /* true is ok */                                                           \
    /* false is failed */                                                      \
    return ret == oldval;                                                      \
  }
ATOMIC_CAS(4,  )
ATOMIC_CAS(8, d)
#undef ATOMIC_CAS

#define WRAP_ATOMIC_OP_VOID(name, order, pre, post)                            \
  template<typename T, typename V>  inline                                     \
  void                                                                         \
  name##_##order(T *mem, V value)                                              \
  {                                                                            \
    pre;                                                                       \
    name ## _relaxed(mem, value);                                              \
    post;                                                                      \
  }

#define WRAP_ATOMIC_OP_RET(name, order, pre, post)                             \
  template<typename T, typename V>  inline                                     \
  T                                                                            \
  name##_##order(T *mem, V value)                                              \
  {                                                                            \
    pre;                                                                       \
    T res = name ## _relaxed(mem, value);                                      \
    post;                                                                      \
    return res;                                                                \
  }

#define WRAP_ATOMIC_OP_CAS(name, order, pre, post)                             \
  template<typename T> inline                                                  \
  bool                                                                         \
  name##_##order(T *mem, T oldval, T newval)                                   \
  {                                                                            \
    pre;                                                                       \
    bool res = name ## _relaxed(mem, oldval, newval);                          \
    post;                                                                      \
    return res;                                                                \
  }

#define ATOMIC_IMPL_VARIANTS(OP_MACRO, name)                                   \
  OP_MACRO(name, acquire,             , Mem::mp_mb())                          \
  OP_MACRO(name, release, Mem::mp_mb(),             )                          \
  OP_MACRO(name, acq_rel, Mem::mp_mb(), Mem::mp_mb())                          \
  OP_MACRO(name, seq_cst, Mem::mp_mb(), Mem::mp_mb())

ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_VOID, atomic_and)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_VOID, atomic_or)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_VOID, atomic_add)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_fetch_and)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_fetch_or)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_fetch_add)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_and_fetch)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_or_fetch)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_add_fetch)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_RET, atomic_exchange)
ATOMIC_IMPL_VARIANTS(WRAP_ATOMIC_OP_CAS, cas)

#undef ATOMIC_IMPL_VARIANTS
#undef ATOMIC_OP_VOID
#undef ATOMIC_OP_RET
#undef ATOMIC_OP_CAS
// preprocess on

//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

#include "mem.h"

inline
void
local_atomic_and(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_and_relaxed<Mword, Mword>(mem, value);
  Mem::barrier();
}

inline
void
local_atomic_or(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_or_relaxed<Mword, Mword>(mem, value);
  Mem::barrier();
}

inline
void
local_atomic_add(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_add_relaxed<Mword, Mword>(mem, value);
  Mem::barrier();
}

template<typename T> inline
bool
local_cas(T *mem, T oldval, T newval)
{
  Mem::barrier();
  bool res = cas_relaxed(mem, oldval, newval);
  Mem::barrier();
  return res;
}
