INTERFACE [vgic && cpu_virt]:

#include "types.h"
#include "vgic.h"

#include <cxx/bitfield>

class Gic_h_v3 : public Gic_h_mixin<Gic_h_v3>
{
public:
  enum { Version = 3 };

  struct Lr
  {
    enum State
    {
      Empty              = 0,
      Pending            = 1,
      Active             = 2,
      Active_and_pending = 3
    };

    Unsigned64 raw;
    Lr() = default;
    explicit Lr(Unsigned64 v) : raw(v) {}
    CXX_BITFIELD_MEMBER(  0, 31, vid, raw);
    CXX_BITFIELD_MEMBER( 32, 41, pid, raw);
    CXX_BITFIELD_MEMBER( 41, 41, eoi, raw);
    CXX_BITFIELD_MEMBER( 48, 55, prio, raw);
    CXX_BITFIELD_MEMBER( 60, 60, grp1, raw);
    CXX_BITFIELD_MEMBER( 61, 61, hw, raw);
    CXX_BITFIELD_MEMBER( 62, 63, state, raw);
    CXX_BITFIELD_MEMBER( 62, 62, pending, raw);
    CXX_BITFIELD_MEMBER( 63, 63, active, raw);
  };

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

namespace {

struct Gic_h_v3_init
{
  explicit Gic_h_v3_init()
  {
    unsigned v = Pic::gic->gic_version();
    if (v < 3 || v > 4)
      return;

    Gic_h_global::gic = new Boot_object<Gic_h_v3>();
  }
};

Gic_h_v3_init __gic_h;
}

