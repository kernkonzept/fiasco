IMPLEMENTATION [malta && mp]:

#include "ipi.h"

IMPLEMENT_OVERRIDE
void
Platform_control::boot_all_secondary_cpus()
{
  // no IPIs supported cannot boot other CPUs
  if (Ipi::hw == nullptr)
    return;

  if (Cm::present()
      && Cm::cm->cpc_present())
    {
      boot_all_secondary_cpus_cm();
      return;
    }

  // AMON based secondary CPU startup
  struct Cpu_launch
  {
    unsigned long pc;
    unsigned long gp;
    unsigned long sp;
    unsigned long a0;
    unsigned long _pad[3]; /* pad to cache line size to avoid thrashing */
    unsigned long flags;
  };

  extern char _tramp_mp_entry[];
  Cpu_launch volatile *cl = reinterpret_cast<Cpu_launch volatile *>(0x80000f00);

  for (unsigned i = 0; i < 8; ++i)
    {
      if (!(cl[i].flags & 1))
        continue;

      cl[i].pc = reinterpret_cast<Address>(&_tramp_mp_entry);
      asm volatile ("sync" : : : "memory");
      cl[i].flags |= 2;
    }
}
