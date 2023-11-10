IMPLEMENTATION [arm && mp && pf_ti_am]:

#include "psci.h"
#include "cpu.h"
#include "panic.h"
#include "reset.h"
#include "poll_timeout_counter.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  boot_ap_cpus_psci(phys_tramp_mp_addr, { 0, 1 });
}
