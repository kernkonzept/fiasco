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
  extern char __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid;
  switch(sizeof(Lock_t))
    {
    case 1: L(b); break;
    case 2: L(h); break;
    case 4: L(); break;
    default: __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid = 10; break;
    }

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
      : [lock] "=m" (_lock), [tmp] "=&r" (tmp)); \
  Mem::dsb(); \
  __asm__ __volatile__("sev")
  extern char __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid;
  switch (sizeof(Lock_t))
    {
    case 1: UNL(b); break;
    case 2: UNL(h); break;
    case 4: UNL(); break;
    default: __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid = 11; break;
    }
#undef UNL
}

