// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && amp]:

#include "processor.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["processor.h"]
FIASCO_PURE Amp_phys_id
Amp_node::phys_id()
{
  return Amp_phys_id(cxx::int_value<Cpu_phys_id>(Proc::cpu_id()) & 0xffU);
}

