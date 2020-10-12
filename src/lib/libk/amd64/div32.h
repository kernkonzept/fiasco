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
  asm ("divq    %[divisor]      \n\t"
     : "=a"(ret), "=d"(dummy)
     : "a"(dividend), "d"(0), [divisor]"rm"(divisor));
  return ret;
}

#endif
