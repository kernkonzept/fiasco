// ------------------------------------------------------------------------
INTERFACE [arm && pf_fvp_base_r]:

EXTENSION class Platform_control
{
  enum { Max_cores = 4 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && !arm_psci]:

#include "reset.h"

IMPLEMENT_OVERRIDE
void
Platform_control::system_off()
{
  platform_shutdown();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && mp]:

#include "cpu.h"
#include "kmem_mmio.h"
#include "koptions.h"
#include "mem_unit.h"
#include "minmax.h"
#include "mmio_register_block.h"
#include "poll_timeout_counter.h"
#include "spin_lock.h"
#include "warn.h"
#include <lock_guard.h>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_tramp_mp_addr)
{
  {
    // Other CPUs expect that it's safe to read Kmem::kdir. We will modify it
    // when removing the core_spin_addr mapping, though. Protect ourself from
    // the AP CPUs seeing this short mapping!
    extern Spin_lock<Mword> _tramp_mp_spinlock;
    auto g = lock_guard(_tramp_mp_spinlock);

    void *mmio = Kmem_mmio::map(Koptions::o()->core_spin_addr, sizeof(Address));
    Mmio_register_block s(mmio);

    s.r<Address>(0) = phys_tramp_mp_addr;
    Mem::dsb();
    Mem_unit::clean_dcache(s.get_mmio_base());

    // Remove mappings to release precious MPU regions
    Kmem_mmio::unmap(mmio, sizeof(Address));
  }

  for (int i = 1; i < min<int>(Max_cores, Config::Max_num_cpus); ++i)
    {
      // Wake up other cores from WFE in the bootstrap spin-address trampoline.
      asm volatile("sev" : : : "memory");
      L4::Poll_timeout_counter guard(5000000);
      while (guard.test(!Cpu::online(Cpu_number(i))))
        {
          Mem::barrier();
          Proc::pause();
        }

      if (guard.timed_out())
        WARNX(Error, "CPU%d did not show up!\n", i);
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && amp]:

#include "static_init.h"

PUBLIC static void
Platform_control::amp_boot_init()
{ amp_boot_ap_cpus(Max_cores); }

static void
setup_amp()
{ Platform_control::amp_prepare_ap_cpus(); }

STATIC_INITIALIZER_P(setup_amp, EARLY_INIT_PRIO);
