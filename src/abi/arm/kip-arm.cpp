/*
 * ARM Kernel-Info Page
 */

INTERFACE [arm]:

#include "types.h"

EXTENSION class Kip
{
public:
  struct Platform_info
  {
    char name[16];
    Unsigned32 is_mp;
    char arch_cpuinfo[252]; // Just placeholders
  };

  /* 0xF0 */
  Platform_info platform_info;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && debug]:

IMPLEMENT inline
void
Kip::debug_print_syscalls() const
{}
