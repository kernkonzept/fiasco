IMPLEMENTATION [arm && pf_kirkwood]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

class Kirkwood_reset
{
public:
  enum
  {
    Mask_reg       = 0x108,
    Soft_reset_reg = 0x10c,
  };
};


[[noreturn]] void
platform_reset()
{
  Mmio_register_block r(Kmem_mmio::map(Mem_layout::Reset_phys_base, 0x200));
  // enable software reset
  r.write<Unsigned32>(1 << 2, Kirkwood_reset::Mask_reg);

  // do software reset
  r.write<Unsigned32>(1, Kirkwood_reset::Soft_reset_reg);

  L4::infinite_loop();
}
