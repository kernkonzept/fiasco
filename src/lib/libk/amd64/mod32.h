#ifndef MLC_MOD32_H__
#define MLC_MOD32_H__

#include "std_macros.h"

/** return (dividend % divisor) with divisor<2^32 */
static inline FIASCO_CONST
unsigned long
mod32(unsigned long long dividend, unsigned long divisor)
{
  unsigned long ret, dummy;
  asm ("divq    %[divisor]      \n\t"
     : "=d"(ret), "=a"(dummy)
     : "a"(dividend), "d"(0), [divisor]"rm"(divisor));
  return ret;
}

#endif
