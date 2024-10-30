IMPLEMENTATION[ia32 || amd64]:

#include "cpu.h"
#include "config.h"
#include "kip.h"
#include "warn.h"

//----------------------------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && !sync_clock]:

IMPLEMENT inline NEEDS ["config.h", "cpu.h", "kip.h"]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    Kip::k()->add_to_clock(Config::Scheduler_granularity);
}

//----------------------------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && sync_clock]:

IMPLEMENT_OVERRIDE inline NEEDS ["cpu.h", "kip.h", "warn.h"]
void
Timer::init_system_clock()
{
  Cpu_time time = Cpu::cpus.cpu(_cpu).time_us();
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on boot CPU\n", time);
}

IMPLEMENT_OVERRIDE inline NEEDS["cpu.h", "kip.h", "warn.h"]
void
Timer::init_system_clock_ap(Cpu_number cpu)
{
  Cpu_time time = Cpu::cpus.cpu(_cpu).time_us();
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on CPU%u\n",
         time, cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE inline NEEDS ["cpu.h"]
Unsigned64
Timer::system_clock()
{
  return Cpu::cpus.cpu(_cpu).time_us();
}

IMPLEMENT inline NEEDS ["cpu.h"]
Unsigned64
Timer::aux_clock_unstopped()
{
  return Cpu::cpus.cpu(_cpu).time_us();
}
