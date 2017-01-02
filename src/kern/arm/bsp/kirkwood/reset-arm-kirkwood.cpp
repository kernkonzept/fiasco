IMPLEMENTATION [arm && pf_kirkwood]:

#include "io.h"
#include "kmem.h"
#include "mmio_register_block.h"

class Kirkwood_reset
{
public:
  enum
  {
    Mask_reg       = 0x20108,
    Soft_reset_reg = 0x2010c,
  };
};


void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block r(Kmem::mmio_remap(Mem_layout::Reset_phys_base));
  // enable software reset
  r.write(1 << 2, Kirkwood_reset::Mask_reg);

  // do software reset
  r.write(1, Kirkwood_reset::Soft_reset_reg);

  for (;;)
    ;
}
