/*
 * IA-32 Kernel-Info Page
 */

INTERFACE [ux]:

#include "vhw.h"

INTERFACE [ia32 || ux]:

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

//---------------------------------------------------------------------------
IMPLEMENTATION [ux]:

PUBLIC
Vhw_descriptor *
Kip::vhw() const
{
  return reinterpret_cast<Vhw_descriptor*>(((unsigned long)this) + vhw_offset);
}
