INTERFACE [arm && mp && sunxi]:

#include "types.h"

IMPLEMENTATION [arm && mp && sunxi]:

#include "ipi.h"
#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  enum {
    CPUx_base      = 0x40,
    CPUx_offset    = 0x40,
    CPUx_RST_CTRL  = 0,
    GENER_CTRL_REG = 0x184,
    PRIVATE_REG    = 0x1a4,
  };
  Mmio_register_block c(Kmem::mmio_remap(0x01c25c00));
  c.write<Mword>(phys_tramp_mp_addr, 0x1a4);

  unsigned cpu = 1;
  c.write<Mword>(0, CPUx_base + CPUx_offset * cpu + CPUx_RST_CTRL);
  c.clear<Mword>(1 << cpu, GENER_CTRL_REG);
  c.write<Mword>(3, CPUx_base + CPUx_offset * cpu + CPUx_RST_CTRL);

  Ipi::bcast(Ipi::Global_request, Cpu_number::boot_cpu());
}
