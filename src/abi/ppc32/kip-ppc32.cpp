/*
 * PPC32 Kernel-Info Page
 */

INTERFACE [ppc32]:

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
IMPLEMENTATION [ppc32 && debug]:

IMPLEMENT inline
void
Kip::debug_print_syscalls() const
{}
