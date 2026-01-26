INTERFACE [arm]:

// preprocess off
#define ATOMIC_OP(name, op)                                       \
  template<typename T, typename V> inline                         \
  void                                                            \
  atomic_##name(T *mem, V value)                                  \
  {                                                               \
    static_assert(sizeof(T) == 4 || sizeof(T) == 8);              \
    T val = value;                                                \
    Mword tmp, ret;                                               \
                                                                  \
    asm volatile (                                                \
        "1:                                 \n"                   \
        "ldxr    %[tmp], %[mem]             \n"                   \
        #op "    %[tmp], %[tmp], %[val]     \n"                   \
        "stxr    %w[ret], %[tmp], %[mem]    \n"                   \
        "cbnz    %w[ret], 1b                \n"                   \
        : [tmp] "=&r" (tmp), [ret] "=&r" (ret), [mem] "+Q" (*mem) \
        : [val] "r" (val)                                         \
        : "cc");                                                  \
  }
ATOMIC_OP(and, and)
ATOMIC_OP(or, orr)
ATOMIC_OP(add, add)
#undef ATOMIC_OP


#define ATOMIC_FETCH_OP(name, op)                                              \
  template<typename T, typename V> inline                                      \
  T                                                                            \
  atomic_fetch_##name(T *mem, V value)                                         \
  {                                                                            \
    static_assert (sizeof(T) == 4 || sizeof(T) == 8,                           \
                   "invalid size of operand (must be 4 or 8 byte)");           \
    T val = value;                                                             \
    T res, old;                                                                \
    Mword tmp;                                                                 \
                                                                               \
    switch (sizeof(T))                                                         \
      {                                                                        \
      case 4:                                                                  \
        asm (                                                                  \
            "     prfm   pstl1strm, %[mem] \n"                                 \
            "1:   ldxr  %w[old], %[mem] \n"                                    \
            "   " #op "   %w[res], %w[old], %w[val] \n"                        \
            "     stxr  %w[tmp], %w[res], %[mem] \n"                           \
            "     cmp   %[tmp], #0 \n"                                         \
            "     bne   1b "                                                   \
            : [res] "=&r" (res), [old] "=&r" (old), [tmp] "=&r" (tmp),         \
              [mem] "+Q" (*mem)                                                \
            : [val] "r" (val)                                                  \
            : "cc");                                                           \
        return old;                                                            \
                                                                               \
      case 8:                                                                  \
        asm (                                                                  \
            "     prfm   pstl1strm, %[mem] \n"                                 \
            "1:   ldxr   %[old], %[mem] \n"                                    \
            "   " #op "    %[res], %[old], %[val] \n"                          \
            "     stxr   %w[tmp], %[res], %[mem] \n"                           \
            "     cmp    %[tmp], #0 \n"                                        \
            "     bne    1b "                                                  \
            : [res] "=&r" (res), [old] "=&r" (old), [tmp] "=&r" (tmp),         \
              [mem] "+Q" (*mem)                                                \
            : [val] "r" (val)                                                  \
            : "cc");                                                           \
        return old;                                                            \
                                                                               \
      default:                                                                 \
        return T();                                                            \
      }                                                                        \
  }
ATOMIC_FETCH_OP(and, and)
ATOMIC_FETCH_OP(or, orr)
ATOMIC_FETCH_OP(add, add)
#undef ATOMIC_FETCH_OP


#define ATOMIC_OP_FETCH(name, op)                                              \
  template<typename T, typename V> inline                                      \
  T                                                                            \
  atomic_##name##_fetch(T *mem, V value)                                       \
  {                                                                            \
    static_assert (sizeof(T) == 4 || sizeof(T) == 8,                           \
                   "invalid size of operand (must be 4 or 8 byte)");           \
    T val = value;                                                             \
    T res;                                                                     \
    Mword tmp;                                                                 \
                                                                               \
    switch (sizeof(T))                                                         \
      {                                                                        \
      case 4:                                                                  \
        asm (                                                                  \
            "     prfm   pstl1strm, %[mem] \n"                                 \
            "1:   ldxr  %w[res], %[mem] \n"                                    \
            "   " #op "   %w[res], %w[res], %w[val] \n"                        \
            "     stxr  %w[tmp], %w[res], %[mem] \n"                           \
            "     cmp   %[tmp], #0 \n"                                         \
            "     bne   1b "                                                   \
            : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)          \
            : [val] "r" (val)                                                  \
            : "cc");                                                           \
        return res;                                                            \
                                                                               \
      case 8:                                                                  \
        asm (                                                                  \
            "     prfm   pstl1strm, %[mem] \n"                                 \
            "1:   ldxr   %[res], %[mem] \n"                                    \
            "   " #op "    %[res], %[res], %[val] \n"                          \
            "     stxr   %w[tmp], %[res], %[mem] \n"                           \
            "     cmp    %[tmp], #0 \n"                                        \
            "     bne    1b "                                                  \
            : [res] "=&r" (res), [tmp] "=&r" (tmp), [mem] "+Q" (*mem)          \
            : [val] "r" (val)                                                  \
            : "cc");                                                           \
        return res;                                                            \
                                                                               \
      default:                                                                 \
        return T();                                                            \
      }                                                                        \
  }
ATOMIC_OP_FETCH(and, and)
ATOMIC_OP_FETCH(or, orr)
ATOMIC_OP_FETCH(add, add)
#undef ATOMIC_OP_FETCH
// preprocess on

//----------------------------------------------------------------------------
IMPLEMENTATION[arm]:

template<typename T, typename V> inline
T
atomic_exchange(T *mem, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;
  T res;
  Mword tmp;

  switch (sizeof(T))
    {
    case 4:
      asm volatile (
          "     prfm  pstl1strm, [%[mem]] \n"
          "1:   ldxr  %w[res], [%[mem]] \n"
          "     stxr  %w[tmp], %w[val], [%[mem]] \n"
          "     cmp   %w[tmp], #0 \n"
          "     b.ne  1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;

    case 8:
      asm volatile (
          "     prfm   pstl1strm, [%[mem]] \n"
          "1:   ldxr   %x[res], [%[mem]] \n"
          "     stxr   %w[tmp], %x[val], [%[mem]] \n"
          "     cmp    %w[tmp], #0 \n"
          "     b.ne   1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;
    }
}

template< typename T > ALWAYS_INLINE inline
T
atomic_load(T const *p)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T res;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("ldr %w0, %1" : "=r" (res) : "m"(*p));
      return res;

    case 8:
      asm volatile ("ldr %x0, %1" : "=r" (res) : "m"(*p));
      return res;
    }
}

template< typename T, typename V > ALWAYS_INLINE inline
void
atomic_store(T *p, V value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  T val = value;
  switch (sizeof(T))
    {
    case 4:
      asm volatile ("str %w1, %0" : "=m" (*p) : "r" (val));
      break;

    case 8:
      asm volatile ("str %x1, %0" : "=m" (*p) : "r" (val));
      break;
    }
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
