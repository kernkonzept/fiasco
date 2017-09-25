#ifndef MLC_DIV32_H__
#define MLC_DIV32_H__

#include "std_macros.h"

/** return (divident / divisor) with divisor<2^32 */
extern inline FIASCO_CONST
unsigned long long
div32(unsigned long long divident, unsigned long divisor)
{
  unsigned long long ret;
  unsigned long dummy;
  asm ("divl	%5		\n\t"
       "xchg	%%eax, %4	\n\t"
       "divl	%5		\n\t"
       "movl	%4, %%edx	\n\t"
     : "=A"(ret), "=r"(dummy)
     : "a"((unsigned long)(divident >> 32)), "d"(0),
       "1"((unsigned long)(divident & 0xffffffff)), "rm"(divisor));
  return ret;
}

#endif
