#ifndef MLC_DIV32_H__
#define MLC_DIV32_H__

#include "std_macros.h"

/** return (divident / divisor) with divisor<2^32 */
extern inline FIASCO_CONST
unsigned long long
div32(unsigned long long divident, unsigned long divisor)
{
  return divident/divisor;
}

#endif
