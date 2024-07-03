INTERFACE [arm && mp]:

EXTENSION class Spin_lock
{
  static_assert(   sizeof(Lock_t) == 1 || sizeof(Lock_t) == 2
                || sizeof(Lock_t) == 4 || sizeof(Lock_t) == 8,
                "unsupported spin-lock type for ARM");
};

IMPLEMENTATION [arm && mp]:

#include "processor.h"

IMPLEMENT template<typename Lock_t> inline NEEDS["processor.h"]
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

#define LOCK_ARCH(z,u) \
  __asm__ __volatile__ ( \
      "   sevl                                      \n" \
      "   prfm pstl1keep, [%[lock]]                 \n" \
      "1: wfe                                       \n" \
      "   ldaxr" #z "  %" #u "[d], [%[lock]]        \n" \
      "   tst     %x[d], #2                         \n" /* Arch_lock == #2 */ \
      "   bne 1b                                    \n" \
      "   orr   %x[tmp], %x[d], #2                  \n" \
      "   stxr" #z " %w[d], %" #u "[tmp], [%[lock]] \n" \
      "   cbnz  %w[d], 1b                           \n" \
      : [d] "=&r" (dummy), [tmp] "=&r"(tmp), "+m" (_lock) \
      : [lock] "r" (&_lock) \
      : "cc", "memory" \
      )

  switch (sizeof(Lock_t))
    {
    case 1: LOCK_ARCH(b,w); break;
    case 2: LOCK_ARCH(h,w); break;
    case 4: LOCK_ARCH(,w); break;
    case 8: LOCK_ARCH(,x); break;
    }

#undef LOCK_ARCH
}

IMPLEMENT template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  Lock_t tmp;

#define UNLOCK_ARCH(z,u) \
  __asm__ __volatile__( \
      "ldr"#z " %" #u "[tmp], %[lock]              \n" \
      "bic %x[tmp], %x[tmp], #2                    \n" /* Arch_lock == #2 */ \
      "stlr"#z " %" #u "[tmp], %[lock]             \n" \
      : [lock] "+Q" (_lock), [tmp] "=&r" (tmp) : : "memory")

  switch (sizeof(Lock_t))
    {
    case 1: UNLOCK_ARCH(b,w); break;
    case 2: UNLOCK_ARCH(h,w); break;
    case 4: UNLOCK_ARCH(,w); break;
    case 8: UNLOCK_ARCH(,x); break;
    }

#undef UNLOCK_ARCH
}
