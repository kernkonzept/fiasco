INTERFACE [arm]:

// preprocess off
#define ATOMIC_OP_(name, op, size, pfx)                                        \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  void                                                                         \
  atomic_##name(T *mem, V value)                                               \
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
  atomic_fetch_##name(T *mem, V value)                                         \
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
  atomic_##name##_fetch(T *mem, V value)                                       \
  {                                                                            \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    asm volatile (                                                                      \
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
#define ATOMIC_OP(name, op)  \
  ATOMIC_OP_(name, op, 4, w) \
  ATOMIC_OP_(name, op, 8, x)

ATOMIC_OP(and, and)
ATOMIC_OP(or, orr)
ATOMIC_OP(add, add)
#undef ATOMIC_OP_
#undef ATOMIC_OP


#define ATOMIC_EXCHANGE(size, pfx)                                             \
  template<typename T, typename V>                                             \
  requires(sizeof(T) == size) inline                                           \
  T                                                                            \
  atomic_exchange(T *mem, V value)                                             \
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
ATOMIC_EXCHANGE(4, w)
ATOMIC_EXCHANGE(8, x)
#undef ATOMIC_EXCHANGE
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

inline
bool
cas_arch(Mword *m, Mword o, Mword n)
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
