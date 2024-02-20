INTERFACE [vgic && cpu_virt]:

#include "types.h"
#include "vgic.h"

#include <cxx/bitfield>

class Gic_h_v3 : public Gic_h_mixin<Gic_h_v3>
{
public:
  enum { Version = 3 };

  // With GICv3, any access to the vGIC interface by system register will trap
  // (see disable_load_defaults()) unless the current context is in extended
  // vCPU user mode and has the vGIC interface enabled.

  Gic_h_v3() : n_aprs(1U << (vtr().pri_bits() - 4))
  {}

  Address gic_v_address() const override { return 0; }

  // fake pointer to call static functions
  static Gic_h const *const gic;
  unsigned n_aprs;
};

IMPLEMENTATION [vgic && cpu_virt]:

#include "boot_alloc.h"
#include "vgic_global.h"
#include "pic.h"

PUBLIC static inline
void
Gic_h_v3::vgic_barrier()
{
  // eret has implicit isb semantics, so no need here to ensure vgic
  // completion before running user-land
}

PUBLIC
void
Gic_h_v3::switch_to_non_vcpu(From_vgic_mode from_mode) override
{
  if (from_mode == From_vgic_mode::Enabled)
    disable_load_defaults();
  // else:
  //   Nothing to do: Any userland access to the vGIC interface will trap as
  //   disable_load_defaults() was executed after switching away from a context
  //   in extended vCPU mode with vGIC enabled.
}

PUBLIC
void
Gic_h_v3::switch_to_vcpu(Arm_vgic const *g, To_user_mode to_mode,
                         From_vgic_mode from_mode) override
{
  if (to_mode == To_user_mode::Enabled)
    {
      if (!switch_to_vcpu_user(g))
        disable_load_defaults();
    }
  else if (from_mode == From_vgic_mode::Enabled)
    disable_load_defaults();
}

PUBLIC
void
Gic_h_v3::save_and_disable(Arm_vgic *g) override
{
  if (switch_from_vcpu(g) == From_vgic_mode::Enabled)
    disable_load_defaults();
}

PUBLIC inline
void
Gic_h_v3::disable_load_defaults()
{
  Hcr h(0);
  h.en() = 0;
  h.tc() = 1;
  h.tall0() = 1;
  h.tall1() = 1;
  h.tsei() = 1;
  h.tdir() = 1;
  hcr(h);
}

namespace {

struct Gic_h_v3_init
{
  explicit Gic_h_v3_init()
  {
    unsigned v = Pic::gic->gic_version();
    if (v < 3 || v > 4)
      return;

    Gic_h_global::gic = new Boot_object<Gic_h_v3>();

    Gic_h_global::gic->disable();
  }
};

Gic_h_v3_init __gic_h;
}

