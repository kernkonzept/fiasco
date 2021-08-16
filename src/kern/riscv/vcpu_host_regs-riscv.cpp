INTERFACE [riscv]:

#include "l4_types.h"

// This is used as the architecture-specific version
// in the vCPU state structure. This version has to be
// changed / increased whenever any of the data structures
// within the vCPU state changes its layout or its semantics.
enum { Vcpu_arch_version = 0x00 };

/**
 * On RISC-V, in addition to ip and sp, also the gp and tp registers are
 * required for a minimal execution environment, such as the one provided for
 * the vCPU entry handler. In the following they are referred to as the vCPU
 * host mode preserved registers.
 *
 * When the vCPU switches from vCPU host mode to vCPU user mode
 * (see Task::resume_vcpu()), it saves the current values of the vCPU host mode
 * preserved registers into the Vcpu_host_regs in its vCPU state save area.
 *
 * Later, when the vCPU switches back from vCPU user mode to vCPU host mode, it
 * restores the saved host mode registers before invoking the vCPU entry
 * handler (see Thread::fast_return_to_user()).
 *
 * When trapping from vCPU host mode into vCPU host mode the vCPU does not touch
 * preserved registers, which is controlled by the saved field in
 * Vcpu_host_regs.
 *
 * When resuming to vCPU host mode, the vCPU copies the current values of the
 * preserved registers into its state save area.
 */
struct Vcpu_host_regs
{
  Mword tp;
  Mword gp;
  Unsigned8 saved;
};
