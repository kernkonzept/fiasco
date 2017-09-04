IMPLEMENTATION[arm && arm_v6plus]:

#include "mem.h"

inline
void
atomic_mp_add(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldrex   %[v], [%[mem]]             \n"
      "add     %[v], %[v], %[addval]      \n"
      "strex   %[ret], %[v], [%[mem]]     \n"
      "teq     %[ret], #0                 \n"
      "bne     1b                         \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), "+m" (*l)
      :  [mem] "r" (l), [addval] "r" (value)
      : "cc");
}

inline
void
atomic_mp_and(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldrex   %[v], [%[mem]]             \n"
      "and     %[v], %[v], %[andval]     \n"
      "strex   %[ret], %[v], [%[mem]]     \n"
      "teq     %[ret], #0                 \n"
      "bne     1b                         \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), "+m" (*l)
      :  [mem] "r" (l), [andval] "r" (value)
      : "cc");
}

inline
void
atomic_mp_or(Mword *l, Mword value)
{
  Mword tmp, ret;

  asm volatile (
      "1:                                 \n"
      "ldrex   %[v], [%[mem]]             \n"
      "orr     %[v], %[v], %[orval]     \n"
      "strex   %[ret], %[v], [%[mem]]     \n"
      "teq     %[ret], #0                 \n"
      "bne     1b                         \n"
      : [v] "=&r" (tmp), [ret] "=&r" (ret), "+m" (*l)
      :  [mem] "r" (l), [orval] "r" (value)
      : "cc");
}

inline NEEDS["mem.h"]
bool
mp_cas_arch(Mword *m, Mword o, Mword n)
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
      Mem::prefetch_w(mem);
      asm (
          "1:   ldrex %[res], [%[mem]] \n"
          "     strex %[tmp], %[val], [%[mem]] \n"
          "     cmp   %[tmp], #0 \n"
          "     bne   1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;

    case 8:
      Mem::prefetch_w(mem);
      asm (
          "1:   ldrexd %[res], %H[res], [%[mem]] \n"
          "     strexd %[tmp], %[val], %H[val], [%[mem]] \n"
          "     cmp    %[tmp], #0 \n"
          "     bne    1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;

    default:
      return T();
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
      Mem::prefetch_w(mem);
      asm (
          "1:   ldrex %[res], [%[mem]] \n"
          "     add   %[res], %[res], %[val] \n"
          "     strex %[tmp], %[res], [%[mem]] \n"
          "     cmp   %[tmp], #0 \n"
          "     bne   1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;

    case 8:
      Mem::prefetch_w(mem);
      asm (
          "1:   ldrexd %[res], %H[res], [%[mem]] \n"
          "     adds   %[res], %[res], %[val] \n"
          "     adc    %H[res], %H[res], %H[val] \n"
          "     strexd %[tmp], %[val], %H[val], [%[mem]] \n"
          "     cmp    %[tmp], #0 \n"
          "     bne    1b "
          : [res] "=&r" (res), [tmp] "=&r" (tmp), "+Qo" (*mem)
          : [mem] "r" (mem), [val] "r" (val)
          : "cc");
      return res;

    default:
      return T();
    }
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && arm_lpae]:

template< typename T > inline
T ALWAYS_INLINE
atomic_load(T const *p)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_load supported for 4 and 8 byte types only");
  return *const_cast<T const volatile *>(p);
}

template< typename T > inline
void ALWAYS_INLINE
atomic_store(T *p, T value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_store supported for 4 and 8 byte types only");
  *const_cast<T volatile *>(p) = value;
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && !arm_lpae]:

template< typename T > inline
T ALWAYS_INLINE
atomic_load(T const *p)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_load supported for 4 and 8 byte types only");
  switch (sizeof(T))
    {
    case 4:
      return *const_cast<T const volatile *>(p);
    case 8:
        {
          T res;
          asm volatile ("ldrexd %0, %H0, [%1]"
                        : "=&r"(res) : "r"(p), "Qo"(*p));
          return res;
        }
    }
}

template< typename T > inline
void ALWAYS_INLINE
atomic_store(T *p, T value)
{
  static_assert(sizeof(T) == 4 || sizeof(T) == 8,
                "atomic_store supported for 4 and 8 byte types only");
  switch (sizeof(T))
    {
    case 4:
      *const_cast<T volatile *>(p) = value;
      break;
    case 8:
        {
          long long tmp;
          Mem::prefetch_w(p);
          asm volatile (
              "1: ldrexd %0, %H0, [%2] \n"
              "   strexd %0, %3, %H3, [%2] \n"
              "   teq    %0, #0 \n"
              "   bne    1b"
              : "=&r"(tmp), "=Qo"(*p)
              : "r"(p), "r"(value)
              : "cc");
        }
      break;
    }
}


