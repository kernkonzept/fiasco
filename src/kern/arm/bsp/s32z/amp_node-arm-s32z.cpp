// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32z]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 4;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z && pf_s32z_rtu_1]:

IMPLEMENT_OVERRIDE static constexpr
Amp_phys_id
Amp_node::first_node()
{ return Amp_phys_id(4); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32z]:

#include "processor.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["processor.h"]
FIASCO_PURE Amp_phys_id
Amp_node::phys_id()
{
  unsigned mpidr = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());

  // We know that Aff0 is in the range [0..3] and Aff1 in [0..1]...
  return Amp_phys_id(((mpidr >> 6) | mpidr) & 0x07);
}
