INTERFACE:
#include "types.h"

class Util
{};

IMPLEMENTATION:

PUBLIC static inline
template < typename T >
T
Util::log2(T value)
{
  unsigned long c = 0;
  while(value >>= 1)
    c++;
  return T(c);
}
