IMPLEMENTATION [arm && pf_xscale]:

#include "infinite_loop.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset(void)
{
  enum
  {
    OSCR  = 0x10,
    OWER  = 0x18,
  };

  Mmio_register_block timer(Kmem_mmio::map(Mem_layout::Timer_phys_base, 0x20));

  timer.write<Mword>(1, OWER);
  timer.write<Mword>(0xffffff00, OSCR);

  L4::infinite_loop();
}
