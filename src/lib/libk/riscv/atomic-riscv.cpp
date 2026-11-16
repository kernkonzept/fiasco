INTERFACE [riscv]:

// preprocess off
#define ATOMIC_LOAD_OP_(size, suffix, order_name, pre_fence, post_fence, cl)   \
  template<typename T>                                                         \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_load##order_name(T const *mem)                                        \
  {                                                                            \
    T res;                                                                     \
    asm volatile (                                                             \
        pre_fence " \n"                                                        \
        "l"#suffix " %[res], %[mem] \n"                                        \
        post_fence " \n"                                                       \
        : [res] "=r" (res)                                                     \
        : [mem] "m"(*mem)                                                      \
        : cl);                                                                 \
    return res;                                                                \
  }

#define ATOMIC_LOAD_OP(order_name, pre_fence, post_fence, cl)                  \
  ATOMIC_LOAD_OP_(4, w, order_name, pre_fence, post_fence, cl)                 \
  ATOMIC_LOAD_OP_(8, d, order_name, pre_fence, post_fence, cl)
ATOMIC_LOAD_OP(_relaxed,               ,              ,         )
ATOMIC_LOAD_OP(_acquire,               , "fence r, rw", "memory")
ATOMIC_LOAD_OP(_seq_cst, "fence rw, rw", "fence r, rw", "memory")
#undef ATOMIC_LOAD_OP_
#undef ATOMIC_LOAD_OP


#define ATOMIC_STORE_OP_(size, suffix, order_name, pre_fence, post_fence, cl)  \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  void                                                                         \
  atomic_store##order_name(T *mem, V value)                                    \
  {                                                                            \
    T val = value;                                                             \
    asm volatile (                                                             \
        pre_fence " \n"                                                        \
        "s"#suffix " %[val], %[mem] \n"                                        \
        post_fence " \n"                                                       \
        : [mem] "=m" (*mem)                                                    \
        : [val] "r" (val)                                                      \
        : cl);                                                                 \
  }

#define ATOMIC_STORE_OP(order_name, pre_fence, post_fence, cl)                 \
  ATOMIC_STORE_OP_(4, w, order_name, pre_fence, post_fence, cl)                \
  ATOMIC_STORE_OP_(8, d, order_name, pre_fence, post_fence, cl)
ATOMIC_STORE_OP(_relaxed,              ,               ,         )
ATOMIC_STORE_OP(_release, "fence rw, w",               , "memory")
ATOMIC_STORE_OP(_seq_cst, "fence rw, w", "fence rw, rw", "memory")
#undef ATOMIC_STORE_OP_
#undef ATOMIC_STORE_OP


#define ATOMIC_VARIANTS(OP_MACRO, ...)                                         \
  OP_MACRO(__VA_ARGS__, _relaxed,      ,         )                             \
  OP_MACRO(__VA_ARGS__, _acquire, .aq  , "memory")                             \
  OP_MACRO(__VA_ARGS__, _release, .rl  , "memory")                             \
  OP_MACRO(__VA_ARGS__, _acq_rel, .aqrl, "memory")                             \
  OP_MACRO(__VA_ARGS__, _seq_cst, .aqrl, "memory")

#define ATOMIC_OP_(op, size, suffix, order_name, order, cl)                    \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  void                                                                         \
  atomic_##op##order_name(T *mem, V value)                                     \
  {                                                                            \
    static_assert(cxx::is_integral_v<T>);                                      \
    T val = value;                                                             \
                                                                               \
    asm volatile (                                                             \
      "amo" #op "." #suffix #order " zero, %[mask], %[mem]"                    \
      : [mem]"+A"(*mem)                                                        \
      : [mask]"r" (val)                                                        \
      : cl);                                                                   \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_fetch_##op##order_name(T *mem, V value)                               \
  {                                                                            \
    static_assert(cxx::is_integral_v<T>);                                      \
    T val = value;                                                             \
    T prev;                                                                    \
                                                                               \
    asm volatile (                                                             \
      "amo" #op "." #suffix #order " %[prev], %[mask], %[mem]"                 \
      : [prev]"=r" (prev), [mem]"+A"(*mem)                                     \
      : [mask]"r" (val)                                                        \
      : cl);                                                                   \
    return prev;                                                               \
  }                                                                            \
                                                                               \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_##op##_fetch##order_name(T *mem, V value)                             \
  {                                                                            \
    static_assert(cxx::is_integral_v<T>);                                      \
    T val = value;                                                             \
    T res;                                                                     \
                                                                               \
    asm volatile (                                                             \
      "amo" #op "." #suffix #order " %[res], %[val], %[mem] \n"                \
      : [res]"=r" (res), [mem]"+A"(*mem)                                       \
      : [val]"r" (val)                                                         \
      : cl);                                                                   \
                                                                               \
    asm (#op " %[res], %[res], %[val] \n"                                      \
         : [res] "+r" (res)                                                    \
         : [val] "r" (val));                                                   \
    return res;                                                                \
  }

#define ATOMIC_OP(op)                                                          \
  ATOMIC_VARIANTS(ATOMIC_OP_, op, 4, w)                                        \
  ATOMIC_VARIANTS(ATOMIC_OP_, op, 8, d)

ATOMIC_OP(or)
ATOMIC_OP(and)
ATOMIC_OP(add)
#undef ATOMIC_OP_
#undef ATOMIC_OP


#define ATOMIC_EXCHANGE_OP(size, suffix, order_name, order, cl)                \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  T                                                                            \
  atomic_exchange##order_name(T *mem, V value)                                 \
  {                                                                            \
    T val = value;                                                             \
    T prev;                                                                    \
                                                                               \
    asm volatile (                                                             \
      "amoswap." #suffix #order " %[prev], %[mask], %[mem]"                    \
      : [prev]"=r" (prev), [mem]"+A"(*mem)                                     \
      : [mask]"r" (val)                                                        \
      : cl);                                                                   \
    return prev;                                                               \
  }
ATOMIC_VARIANTS(ATOMIC_EXCHANGE_OP, 4, w)
ATOMIC_VARIANTS(ATOMIC_EXCHANGE_OP, 8, d)
#undef ATOMIC_EXCHANGE_OP
#undef ATOMIC_VARIANTS


#define ATOMIC_CAS_OP_(size, suffix, order_name, ol, os, cl)                   \
  template<typename T>                                                         \
  requires(sizeof(T) == size && size <= sizeof(Mword)) inline                  \
  bool                                                                         \
  cas##order_name(T *mem, T oldval, T newval)                                  \
  {                                                                            \
    T prev;                                                                    \
    /* Holds return value of SC instruction: 0 if successful, !0 otherwise */  \
    Mword ret = 1;                                                             \
                                                                               \
    asm volatile (                                                             \
      "0:                                     \n"                              \
      "lr." #suffix #ol " %[prev], %[mem]            \n"                       \
      "bne         %[prev], %[oldval], 1f     \n"                              \
      "sc." #suffix #os " %[ret],  %[newval], %[mem] \n"                       \
      "bnez        %[ret],  0b                \n"                              \
      "1:                                     \n"                              \
      : [prev]"=&r" (prev), [ret]"+&r" (ret), [mem]"+A" (*mem)                 \
      : [newval]"r" (newval), [oldval]"r" (oldval)                             \
      : cl);                                                                   \
                                                                               \
    return ret == 0;                                                           \
  }

#define ATOMIC_CAS_OP(order_name, ol, os, cl)                                  \
  ATOMIC_CAS_OP_(4, w, order_name, ol, os, cl)                                 \
  ATOMIC_CAS_OP_(8, d, order_name, ol, os, cl)

ATOMIC_CAS_OP(_relaxed,      ,    ,         )
ATOMIC_CAS_OP(_acquire, .aq  ,    , "memory")
ATOMIC_CAS_OP(_release,      , .rl, "memory")
ATOMIC_CAS_OP(_acq_rel, .aq  , .rl, "memory")
ATOMIC_CAS_OP(_seq_cst, .aqrl, .rl, "memory")
#undef ATOMIC_CAS_OP
#undef ATOMIC_CAS_OP_
// preprocess on

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "mem.h"

inline NEEDS["mem.h"]
void
local_atomic_and(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_and_relaxed(mem, value);
  Mem::barrier();
}

inline NEEDS["mem.h"]
void
local_atomic_or(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_or_relaxed(mem, value);
  Mem::barrier();
}

inline NEEDS["mem.h"]
void
local_atomic_add(Mword *mem, Mword value)
{
  Mem::barrier();
  atomic_add_relaxed(mem, value);
  Mem::barrier();
}

template<typename T> inline NEEDS["mem.h"]
bool
local_cas(T *mem, T oldval, T newval)
{
  Mem::barrier();
  bool res = cas_relaxed(mem, oldval, newval);
  Mem::barrier();
  return res;
}
