//---------------------------------------------------------------------------
INTERFACE [arm && mp]:

EXTENSION class Spin_lock
{
public:
  enum { Arch_lock = 2 };
};
//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mp]:

#include "processor.h"

PRIVATE template<typename Lock_t> inline NEEDS["processor.h"]
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

#define L(z) \
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
  if (sizeof(Lock_t) == sizeof(char))
    L(b);
  else if (sizeof(Lock_t) == sizeof(short))
    L(h);
  else
    L();

#undef L
}

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  Lock_t tmp;
#define UNL(z) \
  __asm__ __volatile__( \
      "ldr"#z " %[tmp], %[lock]             \n" \
      "bic %[tmp], %[tmp], #2          \n" /* Arch_lock == #2 */ \
      "str"#z " %[tmp], %[lock]             \n" \
      "mcr p15, 0, %[tmp], c7, c10, 4  \n" /* drain write buffer */ \
      "sev                             \n" \
      : [lock] "=m" (_lock), [tmp] "=&r" (tmp))
  if (sizeof(Lock_t) == sizeof(char))
    UNL(b);
  else if (sizeof(Lock_t) == sizeof(short))
    UNL(h);
  else
    UNL();
#undef UNL
}
