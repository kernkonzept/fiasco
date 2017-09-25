IMPLEMENTATION [arm && pxa]:

#include "mem_layout.h"
#include "mmio_register_block.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum {
    OSCR  = 0x10,
    OWER  = 0x18,
  };

  Mmio_register_block timer(Kmem::mmio_remap(Mem_layout::Timer_phys_base));

  timer.write<Mword>(1, OWER);
  timer.write<Mword>(0xffffff00, OSCR);

  for (;;)
    ;
}
