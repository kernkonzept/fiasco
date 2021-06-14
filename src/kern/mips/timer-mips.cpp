INTERFACE [mips]:

#include "per_cpu_data.h"
#include "irq_chip.h"

EXTENSION class Timer
{
private:
  Unsigned32 _interval;
  Unsigned32 _current_cmp;
  Unsigned32 _last_counter;
  Unsigned32 _counter_high;

  static Per_cpu<Static_object<Timer> > _timer;

  enum
  {
    // Chose a reasonable amount of cycles within that we will be able to
    // reprogram the compare register without being 'caught up' by the counter
    // register.
    Adj_time = 8000
  };
};

IMPLEMENTATION [mips]:

#include "cpu.h"
#include "config.h"
#include "globals.h"
#include "kip.h"
#include "warn.h"
#include "irq_chip.h"

DEFINE_PER_CPU Per_cpu<Static_object<Timer> > Timer::_timer;

PRIVATE static inline
Unsigned32
Timer::_get_compare()
{ return Mips::mfc0_32(Mips::Cp0_compare); }

PRIVATE static inline
void
Timer::_set_compare(Unsigned32 v)
{ Mips::mtc0_32(v, Mips::Cp0_compare); }

PRIVATE static inline
Unsigned32
Timer::_get_counter()
{ return Mips::mfc0_32(Mips::Cp0_count); }

PRIVATE static inline
void
Timer::_set_counter(Unsigned32 cnt)
{ Mips::mtc0_32(cnt, Mips::Cp0_count); }

IMPLEMENT
void
Timer::init(Cpu_number ncpu)
{
  if (ncpu == Cpu_number::boot_cpu())
    init_system_clock();

  Cpu &cpu = Cpu::cpus.cpu(ncpu);
  Timer *t = _timer.cpu(ncpu).construct();
  t->_current_cmp = _get_counter();
  t->_last_counter = t->_current_cmp;
  t->_counter_high = 0;
  // Timer is clocked with half the CPU frequency
  t->_interval = (cpu.frequency() / 1000000) * (Config::Scheduler_granularity / 2);

  if (true) // interval mode
    t->_current_cmp += t->_interval;

  for (;;)
    {
      _set_compare(t->_current_cmp);
      // See explanation in Timer::acknowledge().
      Unsigned32 cnt = _get_counter();
      if (EXPECT_TRUE((Signed32)(t->_current_cmp - cnt) > 0))
        break;
      t->_current_cmp = cnt + Adj_time;
      t->_last_counter = t->_current_cmp;
    }
}

PUBLIC static inline NEEDS["irq_chip.h"]
Irq_chip::Mode
Timer::irq_mode()
{ return Irq_chip::Mode::F_raising_edge; }

PUBLIC static // inline NEEDS[Timer::_set_compare]
void
Timer::acknowledge()
{
  Timer *t = _timer.current();

  if (true) // interval mode
    t->_current_cmp += t->_interval;

  Unsigned32 cnt = _get_counter();
  if (cnt < t->_last_counter)
    ++t->_counter_high;

  t->_last_counter = cnt;

  Unsigned32 new_cmp = t->_current_cmp;
  for (;;)
    {
      // clear TI bit and set new value if applicable
      _set_compare(new_cmp);
      // Now verify that the compare register was indeed set beyond the counter
      // because otherwise it will take a complete wrap around of the counter
      // until the next timer interrupt is generated. This could happen on QEMU
      // or in a VM due to preemption but also on real hardware after we
      // entered + left the kernel debugger. In the latter case we probably
      // need several timer periods until _current_cmp got in sync with the
      // counter again. Note that we don't update _current_cmp here because
      // otherwise we would skip timer interrupts.
      cnt = _get_counter();
      if (EXPECT_TRUE((Signed32)(new_cmp - cnt) > 0))
        break;
      new_cmp = cnt + Adj_time;
    }

  // we don't care about CP0 hazards here as a possible IRQ
  // enable afterwards will clear those anyways
  // printf("TI: %u %u %u\n", cnt, t->_current_cmp, t->_interval);
}

PUBLIC static inline NEEDS[Timer::_get_counter]
Unsigned64
Timer::get_current_counter()
{
  Timer *t = _timer.current();
  Unsigned32 cc = _get_counter();
  Unsigned32 hi = t->_counter_high;
  if (cc < t->_last_counter)
    ++hi;

  return (((Unsigned64)hi) << 32) | cc;
}

IMPLEMENT inline NEEDS ["kip.h"]
void
Timer::init_system_clock()
{
  // FIXME: may be sync to counter register
  Kip::k()->clock(0);
}

IMPLEMENT inline NEEDS ["kip.h"]
Unsigned64
Timer::system_clock()
{
  // FIXME: use local counter directly
  return Kip::k()->clock();
}

IMPLEMENT inline NEEDS ["config.h", "kip.h"]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    Kip::k()->add_to_clock(Config::Scheduler_granularity);
}

IMPLEMENT inline
void
Timer::update_timer(Unsigned64 wakeup)
{
  (void) wakeup;
  // FIXEME: need this for one-shot mode
}

PUBLIC static inline
unsigned
Timer::irq()
{ return (Mips::mfc0_32(12, 1) >> 29) & 0x7; }

