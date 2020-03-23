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

  static_assert(sizeof(Vm_info) <= 0x200, "Vm_info must less than 0x200 bytes");
};

struct Context_hyp
{
  Unsigned64 par;
  Unsigned64 hcr = Cpu::Hcr_non_vm_bits;

  Unsigned64 cntvoff;
  Unsigned64 cntv_cval;
  Unsigned32 cntkctl = 0x3; // allow usr access per default
  Unsigned32 cntv_ctl;

  Mword tpidrprw;
  Unsigned32 contextidr;
};
