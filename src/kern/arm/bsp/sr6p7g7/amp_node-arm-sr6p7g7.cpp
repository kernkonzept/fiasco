// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_sr6p7g7]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 6;
};
// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_sr6p7g7]:

#include "processor.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["processor.h"]
FIASCO_PURE Amp_phys_id
Amp_node::phys_id()
{
  unsigned mpidr = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());

  // We know that Aff0 is in the range [0..1] and Aff1 in [0..2]...
  return Amp_phys_id(((mpidr >> 7) | mpidr) & 0x7);
}
