/*
 * IA-32 Kernel-Info Page
 */

INTERFACE [ia32]:

#include "types.h"

EXTENSION class Kip
{
public:
  struct Platform_info
  {
    char name[16];
    Unsigned32 is_mp;
  };

  /* 0xF0 */
  Platform_info platform_info;
  Unsigned32 __reserved[3];
};
