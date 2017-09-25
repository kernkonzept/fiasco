INTERFACE [arm && mp && imx6]:

#include "types.h"

IMPLEMENTATION [arm && mp && imx6]:

#include "ipi.h"
#include "mem_layout.h"
#include "mmio_register_block.h"
#include "kmem.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Register_block<32> src(Kmem::mmio_remap(Mem_layout::Src_phys_base));
  enum
  {
    SRC_SCR  = 0,
    SRC_GPR3 = 0x28,
    SRC_GPR5 = 0x30,
    SRC_GPR7 = 0x38,

    SRC_SCR_CORE1_3_ENABLE = 7 << 22,
    SRC_SCR_CORE1_3_RESET  = 7 << 14,
  };

  src[SRC_GPR3] = phys_tramp_mp_addr;
  src[SRC_GPR5] = phys_tramp_mp_addr;
  src[SRC_GPR7] = phys_tramp_mp_addr;

  src[SRC_SCR].set(SRC_SCR_CORE1_3_ENABLE | SRC_SCR_CORE1_3_RESET);
}
