IMPLEMENTATION [arm && mp && pf_zynqmp]:

#include "cpu.h"
#include "mem.h"
#include "mmio_register_block.h"
#include "kmem.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  for (int i = 0; i < 4; ++i)
    {
      unsigned coreid[4] = { 0x0, 0x1, 0x2, 0x3 };
      int r = cpu_on(coreid[i], phys_tramp_mp_addr);
      if (r)
        {
          if (r != Psci_already_on)
            printf("CPU%d boot-up error: %d\n", i, r);
          continue;
        }

      // The Zynq-MP firmware will not boot all CPUs if we fire CPU_ON
      // events too fast, thus wait for each CPU to appear so that we do not
      // overburden the firmware.
      while (!Cpu::online(Cpu_number(i)))
        {
          Mem::barrier();
          Proc::pause();
        }
    }
}
