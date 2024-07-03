INTERFACE [arm && mp]:

EXTENSION class Spin_lock
{
  static_assert(   sizeof(Lock_t) == 1 || sizeof(Lock_t) == 2
                || sizeof(Lock_t) == 4,
                "unsupported spin-lock type for ARM");
};

IMPLEMENTATION [arm && mp && !arm_v8plus]:

#include "processor.h"

IMPLEMENT template<typename Lock_t> inline NEEDS["processor.h"]
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

#define LOCK_ARCH(z) \
  __asm__ __volatile__ ( \
      "1: ldrex"#z"   %[d], %[lock]          \n" \
      "   tst     %[d], #2                   \n" /* Arch_lock == #2 */ \
      "   wfene                              \n" \
      "   orreq   %[tmp], %[d], #2           \n" \
      "   strex"#z"eq %[d], %[tmp], %[lock]  \n" \
      "   teqeq   %[d], #0                   \n" \
      "   bne     1b                         \n" \
      : [d] "=&r" (dummy), [tmp] "=&r"(tmp), [lock] "+Q" (_lock) \
      : : "cc" \
      )

  switch(sizeof(Lock_t))
    {
    case 1: LOCK_ARCH(b); break;
    case 2: LOCK_ARCH(h); break;
    case 4: LOCK_ARCH(); break;
    }

  Mem::mp_acquire();

#undef LOCK_ARCH
}

IMPLEMENT template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  Lock_t tmp;

  Mem::mp_release();

#define UNLOCK_ARCH(z) \
  __asm__ __volatile__( \
      "ldr"#z " %[tmp], %[lock]             \n" \
      "bic %[tmp], %[tmp], #2          \n" /* Arch_lock == #2 */ \
      "str"#z " %[tmp], %[lock]             \n" \
      : [lock] "+m" (_lock), [tmp] "=&r" (tmp)); \
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

IMPLEMENTATION [arm && mp && arm_v8plus]:

#include "processor.h"

IMPLEMENT template<typename Lock_t> inline NEEDS["processor.h"]
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

#define LOCK_ARCH(z) \
  __asm__ __volatile__ ( \
      "1: ldaex"#z"   %[d], %[lock]          \n" \
      "   tst     %[d], #2                   \n" \
      "   wfene                              \n" \
      "   orreq   %[tmp], %[d], #2           \n" \
      "   strex"#z"eq %[d], %[tmp], %[lock]  \n" \
      "   teqeq   %[d], #0                   \n" \
      "   bne     1b                         \n" \
      : [d] "=&r" (dummy), [tmp] "=&r"(tmp), [lock] "+Q" (_lock) \
      : : "cc", "memory" \
      )

  switch(sizeof(Lock_t))
    {
    case 1: LOCK_ARCH(b); break;
    case 2: LOCK_ARCH(h); break;
    case 4: LOCK_ARCH(); break;
    }

#undef LOCK_ARCH
}

IMPLEMENT template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  Lock_t tmp;

#define UNLOCK_ARCH(z) \
  __asm__ __volatile__( \
      "ldr"#z " %[tmp], %[lock]  \n" \
      "bic %[tmp], %[tmp], #2    \n" /* Arch_lock == #2 */ \
      "stl"#z " %[tmp], %[lock]  \n" \
      : [lock] "+Q" (_lock), [tmp] "=&r" (tmp) \
      : : "memory" \
      )

  switch (sizeof(Lock_t))
    {
    case 1: UNLOCK_ARCH(b); break;
    case 2: UNLOCK_ARCH(h); break;
    case 4: UNLOCK_ARCH(); break;
    }

#undef UNLOCK_ARCH
}

