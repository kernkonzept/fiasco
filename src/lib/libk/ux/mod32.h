#ifndef MLC_MOD32_H__
#define MLC_MOD32_H__

#include "std_macros.h"

/** return (dividend % divisor) with divisor<2^32 */
static inline FIASCO_CONST
unsigned long
mod32(unsigned long long dividend, unsigned long divisor)
{
  unsigned long ret, dummy;
  asm ("divl    %[divisor]      \n\t"
     : "=d"(ret), "=a"(dummy)
     : "A"(dividend), [divisor]"rm"(divisor));
  return ret;
}

#endif
