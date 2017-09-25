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
    struct
    {
      struct
      {
        Unsigned32 MIDR, CTR, TCMTR, TLBTR, MPIDR, REVIDR;
        Unsigned32 ID_PFR[2], ID_DFR0, ID_AFR0, ID_MMFR[4], ID_ISAR[6];
      } cpuinfo;
    } arch;
    Unsigned32 pad[3];
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
