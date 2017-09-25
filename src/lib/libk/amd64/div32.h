#ifndef MLC_DIV32_H__
#define MLC_DIV32_H__

#include "std_macros.h"

/** return (divident / divisor) with divisor<2^32 */
extern inline FIASCO_CONST
unsigned long long
div32(unsigned long long divident, unsigned long divisor)
{
  unsigned long long ret;
  asm ("divq	%3		\n\t"
     : "=a"(ret)
     : "a"((unsigned long)(divident)), "d"(0), "rm"(divisor));
  return ret;
}

#endif
