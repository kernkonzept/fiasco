#ifndef MLC_DIV32_H__
#define MLC_DIV32_H__

#include "std_macros.h"

/** return (dividend / divisor) with divisor<2^32 */
static inline FIASCO_CONST
unsigned long long
div32(unsigned long long dividend, unsigned long divisor)
{
  unsigned long long ret;
  asm ("divq	%3		\n\t"
     : "=a"(ret)
     : "a"((unsigned long)(dividend)), "d"(0), "rm"(divisor));
  return ret;
}

#endif
