// ------------------------------------------------------------------------
INTERFACE [arm && pf_mps3_an536 && amp]:

EXTENSION class Amp_node
{
public:
  static constexpr Amp_phys_id First_node = Amp_phys_id(0);
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_mps3_an536 && amp]:

#include "processor.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["processor.h"]
FIASCO_PURE Amp_phys_id
Amp_node::phys_id()
{
  return Amp_phys_id(cxx::int_value<Cpu_phys_id>(Proc::cpu_id() & 0xffU));
}
