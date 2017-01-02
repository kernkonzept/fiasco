INTERFACE [arm && mp && pf_realview]:
#include "types.h"

IMPLEMENTATION [arm && mp && pf_realview]:

#include "io.h"
#include "ipi.h"
#include "platform.h"

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  // set physical start address for AP CPUs
  Platform::sys->write<Mword>(0xffffffff, Platform::Sys::Flags_clr);
  Platform::sys->write<Mword>(phys_tramp_mp_addr, Platform::Sys::Flags);

  // wake up AP CPUs, always from CPU 0
  Ipi::bcast(Ipi::Global_request, Cpu_number::boot_cpu());
}

