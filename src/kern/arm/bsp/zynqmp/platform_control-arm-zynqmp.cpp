IMPLEMENTATION [arm && mp && pf_zynqmp]:

#include "cpu.h"
#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"
#include "psci.h"
#include "minmax.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  // The Zynq-MP firmware will not boot all CPUs if we fire CPU_ON
  // events too fast, thus boot sequentially waiting for each core to appear
  // so that we do not overburden the firmware.
  boot_ap_cpus_psci(phys_tramp_mp_addr, { 0x0, 0x1, 0x2, 0x3 }, true);
}
