IMPLEMENTATION [arm && mp]:

#include "processor.h"

PRIVATE template<typename Lock_t> inline NEEDS["processor.h"]
void
Spin_lock<Lock_t>::lock_arch()
{
  Lock_t dummy, tmp;

#define L(z,u) \
  __asm__ __volatile__ ( \
      "1: ldr" #z "     %" #u "[d], [%[lock]]       \n" \
      "   tst     %[d], #2                          \n" /* Arch_lock == #2 */ \
      "   beq 2f                                    \n" \
      "   wfe                                       \n" \
      "   b     1b                                  \n" \
      "2: ldxr" #z "   %" #u "[d], [%[lock]]        \n" \
      "   tst   %[d], #2                            \n" \
      "   bne   1b                                  \n" \
      "   orr   %[tmp], %[d], #2                    \n" \
      "   stxr" #z " %w[d], %" #u "[tmp], [%[lock]] \n" \
      "   cbnz  %w[d], 1b                           \n" \
      : [d] "=&r" (dummy), [tmp] "=&r"(tmp), "+m" (_lock) \
      : [lock] "r" (&_lock) \
      : "cc" \
      )
  extern char __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid;
  switch(sizeof(Lock_t))
    {
    case 1: L(b,w); break;
    case 2: L(h,w); break;
    case 4: L(,w); break;
    case 8: L(,); break;
    default: __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid = 10; break;
    }

#undef L
}

PRIVATE template<typename Lock_t> inline
void
Spin_lock<Lock_t>::unlock_arch()
{
  Lock_t tmp;
#define UNL(z,u) \
  __asm__ __volatile__( \
      "ldr"#z " %" #u "[tmp], %[lock]  \n" \
      "bic %[tmp], %[tmp], #2          \n" /* Arch_lock == #2 */ \
      "stlr"#z " %" #u "[tmp], %[lock]             \n" \
      "dsb ishst                       \n" \
      "sev                             \n" \
      : [lock] "=Q" (_lock), [tmp] "=&r" (tmp))
  extern char __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid;
  switch (sizeof(Lock_t))
    {
    case 1: UNL(b,w); break;
    case 2: UNL(h,w); break;
    case 4: UNL(,w); break;
    case 8: UNL(,); break;
    default: __use_of_invalid_type_for_Spin_lock__sizeof_is_invalid = 11; break;
    }
#undef UNL
}
