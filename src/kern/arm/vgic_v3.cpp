INTERFACE [vgic && cpu_virt]:

#include "types.h"
#include "vgic.h"

#include <cxx/bitfield>

class Gic_h_v3 : public Gic_h_mixin<Gic_h_v3>
{
public:
  enum { Version = 3 };

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

