INTERFACE [mips]:

#include "l4_types.h"

// This is uased as the architecture specific version
// in the vCPU state structure. This version has to be
// changed / increased whenever any of the data structures
// within the vCPU state changes its layout or its semantics.
enum { Vcpu_arch_version = 0x12 };

struct Vcpu_host_regs
{
  Mword ulr;
};
