IMPLEMENTATION [arm && pf_sa1100]:

#include <sa1100.h>
#include "kmem.h"


void __attribute__ ((noreturn))
platform_reset(void)
{
  Sa1100 sa(Kmem::mmio_remap(Mem_layout::Timer_phys_base));
  sa.write( (Mword)Sa1100::RSRR_SWR, (Address)Sa1100::RSRR );
  for (;;)
    ;
}
