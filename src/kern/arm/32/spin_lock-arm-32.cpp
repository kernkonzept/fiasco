INTERFACE [arm && mp]:

EXTENSION class Spin_lock
{
  static_assert(   sizeof(Lock_t) == 1 || sizeof(Lock_t) == 2
                || sizeof(Lock_t) == 4,
                "unsupported spin-lock type for ARM");
};

IMPLEMENTATION [arm && mp]:

#include "processor.h"

PRIVATE template<typename Lock_t> inline NEEDS["processor.h"]
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

#define LOCK_ARCH(z) \
  __asm__ __volatile__ ( \
      "1: ldr" #z "     %[d], [%[lock]]           \n" \
      "   tst     %[d], #2                  \n" /* Arch_lock == #2 */ \
      "   wfene                             \n" \
      "   bne     1b                        \n" \
      "   ldrex"#z"   %[d], [%[lock]]           \n" \
      "   tst     %[d], #2                  \n" \
      "   orr     %[tmp], %[d], #2          \n" \
      "   strex"#z"eq %[d], %[tmp], [%[lock]]   \n" \
      "   teqeq   %[d], #0                  \n" \
      "   bne     1b                        \n" \
      : [d] "=&r" (dummy), [tmp] "=&r"(tmp), "+m" (_lock) \
      : [lock] "r" (&_lock) \
      : "cc" \
      )

  switch(sizeof(Lock_t))
    {
    case 1: LOCK_ARCH(b); break;
    case 2: LOCK_ARCH(h); break;
    case 4: LOCK_ARCH(); break;
    }

#undef LOCK_ARCH
}

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  Lock_t tmp;

#define UNLOCK_ARCH(z) \
  __asm__ __volatile__( \
      "ldr"#z " %[tmp], %[lock]             \n" \
      "bic %[tmp], %[tmp], #2          \n" /* Arch_lock == #2 */ \
      "str"#z " %[tmp], %[lock]             \n" \
      : [lock] "=m" (_lock), [tmp] "=&r" (tmp)); \
  Mem::dsb(); \
  __asm__ __volatile__("sev")

  switch (sizeof(Lock_t))
    {
    case 1: UNLOCK_ARCH(b); break;
    case 2: UNLOCK_ARCH(h); break;
    case 4: UNLOCK_ARCH(); break;
    }

#undef UNLOCK_ARCH
}

