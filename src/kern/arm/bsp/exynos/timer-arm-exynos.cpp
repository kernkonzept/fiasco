IMPLEMENTATION [arm && mptimer && exynos]: // -----------------------------

#include "config.h"
#include "timer_mct.h"
#include "mem_layout.h"

#include <cstdio>

PRIVATE static
Mword
Timer::interval()
{
  Mct_timer mct(Kmem::mmio_remap(Mem_layout::Mct_phys_base));
  // probably need to select proper clock source for MCT
  Mword timer_start = 0UL;
  unsigned factor = 5;
  Mword sp_c = timer_start
               + Mct_core_timer::Mct_freq / (1000000 / Config::Scheduler_granularity)
	         * (1 << factor);

  mct.write<Mword>(0, Mct_timer::Reg::Cfg);
  mct.write<Mword>(1 << 8, Mct_timer::Reg::Tcon);

  mct.write<Mword>(0, Mct_timer::Reg::Cnt_u);
  mct.write<Mword>(timer_start, Mct_timer::Reg::Cnt_l);
  Mword vc = start_as_counter();
  while (sp_c > mct.read<Mword>(Mct_timer::Reg::Cnt_l))
    ;
  Mword interval = (vc - stop_counter()) >> factor;

  mct.write<Mword>(0, Mct_timer::Reg::Tcon);

  if (0)
    printf("MP-Timer-Interval: %ld\n", interval);

  return interval;
}

INTERFACE [arm && exynos_mct]: // -----------------------------------------

#include "types.h"
#include "per_cpu_data.h"
#include "timer_mct.h"

EXTENSION class Timer : public Mct_core_timer
{
public:
  explicit Timer(Address virt) : Mct_core_timer(virt) {}
  static Static_object<Mct_timer> mct;
  static Per_cpu<Static_object<Timer> > timers;
};

IMPLEMENTATION [arm && exynos_mct]: // ------------------------------------

#include "cpu.h"
#include "io.h"
#include <cstdio>

Static_object<Mct_timer> Timer::mct;
DEFINE_PER_CPU Per_cpu<Static_object<Timer> > Timer::timers;

PUBLIC static
void
Timer::configure(Cpu_number cpu)
{
  timers.cpu(cpu)->Mct_core_timer::configure();
}

IMPLEMENT
void
Timer::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      mct.construct(Kmem::mmio_remap(Mem_layout::Mct_phys_base));
      mct->write<Mword>(0, Mct_timer::Reg::Cfg);
    }
  timers.cpu(cpu).construct(Kmem::mmio_remap(Mem_layout::Mct_phys_base + 0x300
                            + cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id()) * 0x100));
  timers.cpu(cpu)->Mct_core_timer::configure();
}

PRIVATE static
unsigned
Timer::us_to_mct(Unsigned64 d_us)
{
  if (d_us > Maxinterval_us)
    return Maxinterval_mct;

  return d_us * (Mct_freq / 1000000);
}

PUBLIC static
void
Timer::periodic_next_event(Cpu_number cpu, Unsigned64 next_wakeup)
{
  assert(next_wakeup >= Kip::k()->clock);

  Unsigned64 d = next_wakeup - Kip::k()->clock;

  d = (d / Config::Scheduler_granularity) * Config::Scheduler_granularity;

  timers.cpu(cpu)->set_interval(us_to_mct(d));
}

PUBLIC static
void
Timer::periodic_default_freq(Cpu_number cpu)
{
  timers.cpu(cpu)->set_interval(Interval);
}

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 wakeup)
{
  Unsigned64 now = Kip::k()->clock;
  Mword interval_mct;
  if (EXPECT_FALSE(wakeup <= now))
    interval_mct = 1;
  else
    interval_mct = us_to_mct(wakeup - now);

  timers.cpu(current_cpu())->set_interval(interval_mct);
}

IMPLEMENT inline NEEDS["config.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  else
    return Kip::k()->clock;
}
