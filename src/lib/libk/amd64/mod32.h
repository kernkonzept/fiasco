#ifndef MLC_MOD32_H__
#define MLC_MOD32_H__

#include "std_macros.h"

/** return (divident % divisor) with divisor<2^32 */
extern inline FIASCO_CONST
unsigned long
mod32(unsigned long long divident, unsigned long divisor)
{
  unsigned long ret, dummy;
  asm ("divq	%5		\n\t"
       "mov	%4, %%rax	\n\t"
       "divq	%5		\n\t"
     : "=d"(ret), "=a"(dummy)
     : "a"((unsigned long)(divident >> 32)), "d"(0),
       "irm"((unsigned long)(divident & 0xffffffff)), "rm"(divisor));
  return ret;
}

#endif
