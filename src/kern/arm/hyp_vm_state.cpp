INTERFACE:

#include "cpu.h"
#include "types.h"
#include "vgic_global.h"

class Hyp_vm_state
{
public:
  enum { Version = 0 };

  struct Vm_info
  {
    Unsigned8  version;
    Unsigned8  gic_version;
    Unsigned8 _rsvd0[2];
    Unsigned32 features;
    Unsigned32 _rsvd1[14];
    Mword      user_values[8];

    void setup()
    {
      version = Version;
      gic_version = Gic_h_global::gic->version();
      features = 0;
    }
  };

  static_assert(sizeof(Vm_info) <= (  Config::Ext_vcpu_state_offset
                                    - Config::Ext_vcpu_infos_offset),
                "Vm_info must not overlap with Vm_state");
};

struct Context_hyp
{
  Unsigned64 par;

  // This HCR value cannot be directly modified by userland after extended vCPU
  // mode has been enabled for the corresponding Context. Instead, this value is
  // derived from host_regs.hcr
  // - in Context::arch_load_vcpu_kern_state() while entering kernel mode after
  //   leaving extended vCPU user mode, and
  // - in Context::arch_load_vcpu_user_state() before resuming an extended vCPU
  //   in user mode.
  // Furthermore, in Context::arch_vcpu_ext_shutdown() this HCR value is loaded
  // with a host-compatible fixed value.
  Unsigned64 hcr = Cpu::Hcr_non_vm_bits_el0;

  Unsigned64 cntvoff;
  Unsigned64 cntv_cval;
  Unsigned32 cntkctl = 0x3; // allow usr access per default
  Unsigned32 cntv_ctl;

  Mword tpidrprw;
  Unsigned32 contextidr;
};
