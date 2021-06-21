IMPLEMENTATION[ia32,ux,amd64]:

#include "cpu.h"
#include "config.h"
#include "globals.h"
#include "kip.h"
#include "warn.h"

IMPLEMENT inline NEEDS ["config.h", "cpu.h", "kip.h", "warn.h"]
void
Timer::init_system_clock()
{
  if (Config::Kip_clock_uses_rdtsc)
    {
      Cpu_time time = Cpu::cpus.cpu(_cpu).time_us();
      Kip::k()->set_clock(time);
      if (time >= Kip::Clock_1_year)
        WARN("KIP clock initialized to %llu on boot CPU\n", time);
    }
  else
    Kip::k()->set_clock(0);
}

IMPLEMENT_OVERRIDE inline NEEDS["config.h", "cpu.h", "kip.h", "warn.h"]
void
Timer::init_system_clock_ap(Cpu_number cpu)
{
  if (Config::Kip_clock_uses_rdtsc)
    {
      Cpu_time time = Cpu::cpus.cpu(_cpu).time_us();
      if (time >= Kip::Clock_1_year)
        WARN("KIP clock initialized to %llu on CPU%u\n",
             time, cxx::int_value<Cpu_number>(cpu));
    }
}

IMPLEMENT inline NEEDS ["config.h", "cpu.h", "globals.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (current_cpu() == Cpu_number::boot_cpu()
      && Config::Kip_clock_uses_rdtsc)
    {
      Cpu_time time = Cpu::cpus.cpu(_cpu).time_us();
      Kip::k()->set_clock(time);
      return time;
    }

  return Kip::k()->clock();
}

IMPLEMENT inline NEEDS ["config.h", "cpu.h", "globals.h", "kip.h"]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu != Cpu_number::boot_cpu())
    return;

  if (Config::Kip_clock_uses_rdtsc)
    Kip::k()->set_clock(Cpu::cpus.cpu(Cpu_number::boot_cpu()).time_us());
  else
    Kip::k()->add_to_clock(Config::Scheduler_granularity);
}
