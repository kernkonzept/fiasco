#ifndef SPARC_DIV32_H__
#define SPARC_DIV32_H__

#include "std_macros.h"

/** return (dividend / divisor) with divisor<2^32 */
static inline FIASCO_CONST
unsigned long long
div32(unsigned long long dividend, unsigned long divisor)
{
  return dividend/divisor;
}

#endif
