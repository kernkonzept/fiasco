INTERFACE [arm && mp && zynq]:

#include "types.h"

IMPLEMENTATION [arm && mp && zynq]:

#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Mmio_register_block cpu1boot(Kmem::mmio_remap(0xfffffff0));
  cpu1boot.write<Mword>(phys_tramp_mp_addr, 0);
  Mem::mp_wmb();
  asm volatile("sev");
}
