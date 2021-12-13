IMPLEMENTATION [arm && mp && pf_sbsa]:

#include "acpi.h"
#include "psci.h"
#include "cpu.h"
#include "panic.h"
#include "poll_timeout_counter.h"

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  Unsigned64 boot_mpidr = Cpu::mpidr();
  auto *madt = Acpi::find<Acpi_madt const *>("APIC");
  if (!madt)
    panic("SBSA: no MADT found!\n");

  for (unsigned ap_cpu = 1, i = 0; i < Config::Max_num_cpus; i++)
    {
      auto *gicc = madt->find<Acpi_madt::Gic_cpu_if>(i);
      if (!gicc)
        break;
      if (gicc->mpidr == boot_mpidr)
        continue;
      if (!(gicc->flags & Acpi_madt::Gic_cpu_if::Enabled))
        continue;

      int r = Psci::cpu_on(gicc->mpidr, phys_tramp_mp_addr);
      if (r)
        {
          if (r != Psci::Psci_already_on)
            printf("CPU%u[%d:%llx] boot-up error: %d\n", ap_cpu, i, gicc->mpidr,
                   r);
          continue;
        }

      L4::Poll_timeout_counter timeout(0x80000000);
      while (timeout.test(!Cpu::online(Cpu_number(ap_cpu))))
        {
          Mem::barrier();
          Proc::pause();
        }

      if (timeout.timed_out())
        WARNX(Error, "CPU%u[%d:%llx] did not show up!\n", ap_cpu, i,
              gicc->mpidr);
      else
        ++ap_cpu;
    }
}
