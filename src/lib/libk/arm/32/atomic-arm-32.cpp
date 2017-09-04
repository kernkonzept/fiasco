IMPLEMENTATION[arm && arm_v6plus]:

#include "mem.h"
#include <cxx/type_traits>

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

template<typename T, typename V> inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 4), T>::type ALWAYS_INLINE
atomic_exchange(T *mem, V value)
{
  T val = value;
  T res;
  Mword tmp;

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
}

template<typename T, typename V> inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 4), T>::type ALWAYS_INLINE
atomic_add_fetch(T *mem, V value)
{
  T val = value;
  T res;
  Mword tmp;
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
}

template< typename T > inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 4), T>::type ALWAYS_INLINE
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldr %0, %1" : "=r" (res) : "m"(*p));
  return res;
}

template< typename T, typename V > inline NEEDS [<cxx/type_traits>]
void ALWAYS_INLINE
atomic_store(T *p, V value, typename cxx::enable_if<(sizeof(T) == 4), int>::type = 0)
{
  T val = value;
  asm volatile ("str %1, %0" : "=m" (*p) : "r" (val));
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && arm_lpae]:

#include <cxx/type_traits>

template< typename T > inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldrd %0, %H0, %1" : "=r" (res) : "m"(*p));
  return res;
}

template< typename T, typename V > inline NEEDS [<cxx/type_traits>]
inline void ALWAYS_INLINE
atomic_store(T *p, V value, typename cxx::enable_if<(sizeof(T) == 8), int>::type = 0)
{
  T val = value;
  asm volatile ("strd %1, %H1, %0" : "=m" (*p) : "r" (val));
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && (arm_v7plus || (arm_v6 && mp))]:

#include <cxx/type_traits>

template<typename T, typename V> inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_exchange(T *mem, V value)
{
  T val = value;
  T res;
  Mword tmp;
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
}

template<typename T, typename V> inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_add_fetch(T *mem, V value)
{
  T val = value;
  T res;
  Mword tmp;
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
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && mp && !arm_lpae]:

#include <cxx/type_traits>

template< typename T > inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldrexd %0, %H0, [%1]" : "=&r" (res) : "r" (p), "Qo" (*p));
  return res;
}

template< typename T, typename V > inline NEEDS [<cxx/type_traits>]
inline void ALWAYS_INLINE
atomic_store(T *p, V value, typename cxx::enable_if<(sizeof(T) == 8), int>::type = 0)
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
IMPLEMENTATION[arm && arm_v6 && !mp]:

#include <cxx/type_traits>

template<typename T, typename V> inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_exchange(T *mem, V value)
{
  // NOTE: this version assumes close IRQs during the operation
  T val = value;
  T res;
  Mem::prefetch_w(mem);
  asm (
      "1:   ldrd %[res], %H[res], %[mem] \n"
      "     strd %[val], %H[val], %[mem]   "
      : [res] "=&r" (res), [mem] "+m" (*mem)
      : [val] "r" (val));
  return res;
}

template<typename T, typename V> inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_add_fetch(T *mem, V value)
{
  // NOTE: this version assumes close IRQs during the operation
  T val = value;
  T res;
  Mem::prefetch_w(mem);
  asm (
      "1:   ldrd   %[res], %H[res], %[mem] \n"
      "     adds   %[res], %[res], %[val] \n"
      "     adc    %H[res], %H[res], %H[val] \n"
      "     strd   %[val], %H[val], %[mem]"
      : [res] "=&r" (res), [mem] "+m" (*mem)
      : [val] "r" (val)
      : "cc");
  return res;
}

// --------------------------------------------------------------------
IMPLEMENTATION[arm && arm_v6plus && !mp && !arm_lpae]:

#include <cxx/type_traits>

template< typename T > inline NEEDS [<cxx/type_traits>]
typename cxx::enable_if<(sizeof(T) == 8), T>::type ALWAYS_INLINE
atomic_load(T const *p)
{
  T res;
  asm volatile ("ldrd %0, %H0, %1" : "=&r" (res) : "m" (*p));
  return res;
}

template< typename T, typename V > inline NEEDS [<cxx/type_traits>]
inline void ALWAYS_INLINE
atomic_store(T *p, V value, typename cxx::enable_if<(sizeof(T) == 8), int>::type = 0)
{
  T val = value;
  asm volatile (
      "strd   %1, %H1, %0"
      : "=m"(*p)
      : "r"(val));
}

