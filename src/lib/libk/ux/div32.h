#ifndef MLC_DIV32_H__
#define MLC_DIV32_H__

#include "std_macros.h"

/** return (dividend / divisor) with divisor<2^32 */
extern inline FIASCO_CONST
unsigned long long
div32(unsigned long long dividend, unsigned long divisor)
{
  unsigned long long ret;
  unsigned long dummy;
  asm ("divl	%5		\n\t"
       "xchg	%%eax, %4	\n\t"
       "divl	%5		\n\t"
       "movl	%4, %%edx	\n\t"
     : "=A"(ret), "=r"(dummy)
     : "a"((unsigned long)(dividend >> 32)), "d"(0),
       "1"((unsigned long)(dividend & 0xffffffff)), "rm"(divisor));
  return ret;
}

#endif
