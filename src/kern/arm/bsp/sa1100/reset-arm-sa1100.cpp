IMPLEMENTATION [arm && pf_sa1100]:

#include "infinite_loop.h"
#include <sa1100.h>
#include "kmem_mmio.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Sa1100 sa(Kmem_mmio::map(Mem_layout::Timer_phys_base + Sa1100::RSRR,
                           sizeof(Mword)));
  sa.write<Mword>(Sa1100::RSRR_SWR, 0UL);
  L4::infinite_loop();
}
