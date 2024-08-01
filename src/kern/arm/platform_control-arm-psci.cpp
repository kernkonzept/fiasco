IMPLEMENTATION [arm && arm_psci]:

#include "config.h"
#include "cpu.h"
#include "mem.h"
#include "minmax.h"
#include "processor.h"
#include "psci.h"
#include "timer.h"

#include <cstdio>

PUBLIC static
int
Platform_control::cpu_on(unsigned long target, Address phys_tramp_mp_addr)
{
  return Psci::cpu_on(target, phys_tramp_mp_addr);
}

PUBLIC template<size_t NUM>
static void
Platform_control::boot_ap_cpus_psci(Address phys_tramp_mp_addr,
                                    const unsigned long (&physid_list)[NUM],
                                    bool /* sequential */ = false)
{
  /* The current implementation is booting cores sequentially only */

  unsigned seq = 1;
  for (unsigned long physid : physid_list)
    {
      if (seq >= Config::Max_num_cpus)
        break;

      int r = Psci::cpu_on(physid, phys_tramp_mp_addr);
      if (r)
        {
          if (r != Psci::Psci_already_on)
            printf("CPU%d/%lx boot-up error: %d\n", seq, physid, r);
          continue;
        }

      Unsigned64 timeout = Timer::system_clock() + 500 * 1000;
      while (1)
        {
          if (Cpu::online(Cpu_number(seq)))
            break;

          if (Timer::system_clock() > timeout)
            {
              printf("CPU%d/%lx did not come online.\n", seq, physid);
              break;
            }

          Mem::barrier();
          Proc::pause();
        }
      ++seq;
    }
}

PUBLIC static
void
Platform_control::system_reset()
{
  Psci::system_reset();
}

IMPLEMENT_OVERRIDE
void
Platform_control::system_off()
{
  Psci::system_off();
}
