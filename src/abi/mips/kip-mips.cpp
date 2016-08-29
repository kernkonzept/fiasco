/*
 * MIPS Kernel-Info Page
 */

INTERFACE [mips]:

#include "types.h"

EXTENSION class Kip
{
public:
  struct Platform_info
  {
    char       name[16];
    Unsigned32 is_mp;
    Unsigned32 cpuid;
    Unsigned32 pad[2];
  };

  /* 0xF0   0x1E0 */
  Platform_info platform_info;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [mips && debug]:

IMPLEMENT inline
void
Kip::debug_print_syscalls() const
{}
