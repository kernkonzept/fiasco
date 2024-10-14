IMPLEMENTATION[arm]:

template<typename T, typename V> inline
void
atomic_add(T *mem, V value)
{
  static_assert (sizeof(T) == 4 || sizeof(T) == 8,
                 "invalid size of operand (must be 4 or 8 byte)");

  T val = value;
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldxr    %[tmp], %[mem]             \n"
      "add     %[tmp], %[tmp], %[addval]  \n"
      "stxr    %w[ret], %[tmp], %[mem]    \n"
      "cbnz    %w[ret], 1b                \n"
      : [tmp] "=&r" (tmp), [ret] "=&r" (ret), [mem] "+Q" (*mem)
      : [addval] "r" (val)
      : "cc");
}

template<typename T, typename V> inline
void
atomic_and(T *mem, V value)
{
  static_assert (sizeof(T) == 4 || sizeof(T) == 8,
                 "invalid size of operand (must be 4 or 8 byte)");

  T val = value;
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldxr    %[tmp], %[mem]             \n"
      "and     %[tmp], %[tmp], %[andval]  \n"
      "stxr    %w[ret], %[tmp], %[mem]    \n"
      "cbnz    %w[ret], 1b                \n"
      : [tmp] "=&r" (tmp), [ret] "=&r" (ret), [mem] "+Q" (*mem)
      : [andval] "r" (val)
      : "cc");
}

template<typename T, typename V> inline
void
atomic_or(T *mem, V value)
{
  static_assert (sizeof(T) == 4 || sizeof(T) == 8,
                 "invalid size of operand (must be 4 or 8 byte)");

  T val = value;
  Mword tmp, ret;

  asm volatile (
      "1:                                \n"
      "ldxr    %[tmp], %[mem]            \n"
      "orr     %[tmp], %[tmp], %[orval]  \n"
      "stxr    %w[ret], %[tmp], %[mem]   \n"
      "cbnz    %w[ret], 1b               \n"
      : [tmp] "=&r" (tmp), [ret] "=&r" (ret), [mem] "+Q" (*mem)
      : [orval] "r" (val)
      : "cc");
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

template<typename T, typename V> inline
T
atomic_exchange(T *mem, V value)
{
  static_assert (sizeof(T) == 4 || sizeof(T) == 8,
                 "invalid size of operand (must be 4 or 8 byte)");
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

template<typename T, typename V> inline
T
atomic_add_fetch(T *mem, V value)
{
  static_assert (sizeof(T) == 4 || sizeof(T) == 8,
                 "invalid size of operand (must be 4 or 8 byte)");
  T val = value;
  T res;
  Mword tmp;

  switch (sizeof(T))
    {
    case 4:
      asm volatile (
          "     prfm   pstl1strm, [%[mem]] \n"
          "1:   ldxr  %w[res], [%[mem]] \n"
          "     add   %w[res], %w[res], %w[val] \n"
          "     stxr  %w[tmp], %w[res], [%[mem]] \n"
          "     cmp   %[tmp], #0 \n"
          "     bne   1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;

    case 8:
      asm volatile (
          "     prfm   pstl1strm, [%[mem]] \n"
          "1:   ldxr   %x[res], [%[mem]] \n"
          "     add    %x[res], %x[res], %x[val] \n"
          "     stxr   %w[tmp], %x[res], [%[mem]] \n"
          "     cmp    %[tmp], #0 \n"
          "     bne    1b "
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
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_load supported for 4 and 8 byte types only");
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
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_store supported for 4 and 8 byte types only");
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

