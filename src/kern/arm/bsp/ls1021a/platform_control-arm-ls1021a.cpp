IMPLEMENTATION [arm && mp && ls1021a]: // ---------------------------------

#include "ipi.h"
#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  enum { DCFG_CCSR_SCRATCHRW1 = 0x200 };
  Mmio_register_block devcon(Kmem::mmio_remap(0x01ee0000));
  devcon.r<32>(DCFG_CCSR_SCRATCHRW1) = __builtin_bswap32(phys_tramp_mp_addr);
  Mem::mp_wmb();
  Ipi::bcast(Ipi::Global_request, Cpu_number::boot_cpu());
}
