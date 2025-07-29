INTERFACE [arm && dt]:

#include "dt.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && dt]:

#include "cpu.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"
#include "panic.h"
#include "poll_timeout_counter.h"

PRIVATE static
void
Platform_control::boot_ap_cpus_dt(Dt::Node cpus, Address phys_tramp_mp_addr)
{
  unsigned ap_cpu = 1;

  char const *default_method = cpus.get_prop_str("enable-method");
  cpus.for_each_subnode(
    [&ap_cpu, cpus, default_method, phys_tramp_mp_addr](Dt::Node cpu) -> Dt::Cb
    {
      if (ap_cpu >= Config::Max_num_cpus)
        return Dt::Break;

      if (!cpu.check_device_type("cpu"))
        return Dt::Continue;

      uint64_t reg;
      if (!cpu.get_reg_untranslated(0, &reg))
        {
          WARN("CPUs: '%s': missing reg property\n", cpu.get_name("?"));
          return Dt::Continue;
        }

      // Check for MPIDR match of boot CPU first. The boot CPU node does not
      // necessarily have an "enable-method"...
      if (reg == Cpu::mpidr())
        return Dt::Continue;

      // The enable-method may also be on the parent node...
      char const *method = cpu.get_prop_str("enable-method");
      if (!method)
        method = default_method;
      if (!method)
        {
          WARN("CPUs: '%s': no enable-method\n", cpu.get_name("?"));
          return Dt::Continue;
        }

      if (strcmp(method, "psci") == 0)
        {
          if (boot_ap_cpu_psci(ap_cpu, reg, phys_tramp_mp_addr))
            ++ap_cpu;
        }
      else if (strcmp(method, "spin-table") == 0)
        {
          Unsigned64 spin_addr;
          if (!cpu.get_prop_u64("cpu-release-addr", &spin_addr))
            {
              WARN("CPUs: '%s': no cpu-release-addr\n", cpu.get_name("?"));
              return Dt::Continue;
            }

          if (boot_ap_cpu_spin(ap_cpu, reg, spin_addr, phys_tramp_mp_addr))
            ++ap_cpu;
        }
      else
        WARN("CPUs: '%s': unsupported enable-method: %s\n", cpu.get_name("?"),
             method);

      return Dt::Continue;
    });
}

PRIVATE static
bool
Platform_control::boot_ap_cpu_spin(unsigned ap_cpu, Unsigned64 mpidr,
                                   Address spin_addr,
                                   Address phys_tramp_mp_addr)
{
  // Linux always writes 64 bits...
  Mmio_register_block ra(Kmem_mmio::map(spin_addr, sizeof(Unsigned64),
                                        Kmem_mmio::Map_attr::Cached()));
  if (!ra.get_mmio_base())
    return false;

  ra.r<64>(0) = phys_tramp_mp_addr;
  Mem_unit::clean_dcache(reinterpret_cast<char *>(ra.get_mmio_base()));
  asm volatile("sev");

  L4::Poll_timeout_counter timeout(0x80000000);
  while (timeout.test(!Cpu::online(Cpu_number(ap_cpu))))
    {
      Mem::barrier();
      Proc::pause();
    }

  if (timeout.timed_out())
    {
      WARNX(Error, "CPU%u[%llx] did not show up!\n", ap_cpu, mpidr);
      return false;
    }

  return true;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && dt && arm_psci]:

#include "psci.h"

PRIVATE static
bool
Platform_control::boot_ap_cpu_psci(unsigned ap_cpu, Unsigned64 mpidr,
                                   Address phys_tramp_mp_addr)
{
  if (int r = Psci::cpu_on(mpidr, phys_tramp_mp_addr))
    {
      if (r != Psci::Psci_already_on)
        WARNX(Error, "CPU%u[%llx] boot-up error: %d\n", ap_cpu, mpidr, r);
      return false;
    }

  L4::Poll_timeout_counter timeout(0x80000000);
  while (timeout.test(!Cpu::online(Cpu_number(ap_cpu))))
    {
      Mem::barrier();
      Proc::pause();
    }

  if (timeout.timed_out())
    {
      WARNX(Error, "CPU%u[%llx] did not show up!\n", ap_cpu, mpidr);
      return false;
    }

  return true;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && dt && !arm_psci]:

PRIVATE static
bool
Platform_control::boot_ap_cpu_psci(unsigned ap_cpu, Unsigned64 mpidr, Address)
{
  WARNX(Error, "Cannot boot CPU%u[%llx]! CONFIG_PSCI is disabled.\n", ap_cpu,
        mpidr);
  return false;
}
