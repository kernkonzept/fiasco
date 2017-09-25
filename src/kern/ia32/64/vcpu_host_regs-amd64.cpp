INTERFACE [amd64]:

#include "l4_types.h"

// This is used as the architecture-specific version
// in the vCPU state structure. This version has to be
// changed / increased whenever any of the data structures
// within the vCPU state changes its layout or its semantics.
enum { Vcpu_arch_version = 0x23 };

struct Vcpu_host_regs
{
  Mword fs_base;
  Mword gs_base;
  Unsigned16 ds, es, fs, gs;

  Unsigned16 user_ds32;
  Unsigned16 user_cs64;
  Unsigned16 user_cs32;
};
