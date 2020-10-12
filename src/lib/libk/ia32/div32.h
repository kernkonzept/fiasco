#ifndef MLC_DIV32_H__
#define MLC_DIV32_H__

#include "std_macros.h"

/** return (dividend / divisor) with divisor<2^32 */
static inline FIASCO_CONST
unsigned long long
div32(unsigned long long dividend, unsigned long divisor)
{
  unsigned long long ret;
  unsigned long dummy;
  asm ("divl    %[divisor]              \n\t"
       "xchg    %%eax, %[dividend_lo32] \n\t"
       "divl    %[divisor]              \n\t"
       "movl    %[dividend_lo32], %%edx \n\t"
     : "=A"(ret), "=r"(dummy)
     : "a"((unsigned long)(dividend >> 32)), "d"(0),
       [dividend_lo32]"1"((unsigned long)(dividend & 0xffffffff)),
       [divisor]"rm"(divisor));
  return ret;
}

#endif
