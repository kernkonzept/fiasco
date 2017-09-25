/*
 * AMD64 Kernel-Info Page
 */

INTERFACE [amd64]:

#include "types.h"

EXTENSION class Kip
{
public:
  struct Platform_info
  {
    char name[16];
    Unsigned32 is_mp;
  };

  /* 0x1E0 */
  Platform_info platform_info;
  Unsigned32 __reserved[3];
};

