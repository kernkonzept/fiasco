// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_fvp_base_r && mp]:

#include "cpu.h"
#include "kmem.h"
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
  enum { Max_cores = 4 };

  {
    // Other CPUs expect that it's safe to read Kmem::kdir. We will modify it
    // when removing the core_spin_addr mapping, though. Protect ourself from
    // the AP CPUs seeing this short mapping!
    extern Spin_lock<Mword> _tramp_mp_spinlock;
    auto g = lock_guard(_tramp_mp_spinlock);

    Mmio_register_block s(Kmem::mmio_remap(Koptions::o()->core_spin_addr,
                                           sizeof(Address)));

    s.r<Address>(0) = phys_tramp_mp_addr;
    Mem::dsb();
    Mem_unit::clean_dcache(reinterpret_cast<void *>(s.get_mmio_base()));

    // Remove mappings to release precious MPU regions
    Kmem::mmio_unmap(Koptions::o()->core_spin_addr, sizeof(Address));
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
