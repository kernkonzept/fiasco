INTERFACE[sparc]:

#include "types.h"

EXTENSION class Page
{
public:
  enum Attribs_enum : Mword
  {
    //Cache_mask   = 0x00000078,
    CACHEABLE    = 0x00000080,
    NONCACHEABLE = 0x00000000,
    BUFFERED     = 0x00000000, // Hmm...
  };
};
