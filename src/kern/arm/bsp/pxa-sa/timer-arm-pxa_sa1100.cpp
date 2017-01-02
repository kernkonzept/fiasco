// ------------------------------------------------------------------------
INTERFACE [arm && (pf_sa1100 || pf_xscale)]:

#include "kmem.h"
#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
public:
  static unsigned irq() { return 26; }

private:
  enum {
    OSMR0 = 0x00,
    OSMR1 = 0x04,
    OSMR2 = 0x08,
    OSMR3 = 0x0c,
    OSCR  = 0x10,
    OSSR  = 0x14,
    OWER  = 0x18,
    OIER  = 0x1c,

    Timer_diff = (36864 * Config::Scheduler_granularity) / 10000, // 36864MHz*1ms
  };

  static Static_object<Timer> _timer;
};


// -------------------------------------------------------------
IMPLEMENTATION [arm && (pf_sa1100 || pf_xscale)]:

#include "config.h"
#include "kip.h"
#include "pic.h"

Static_object<Timer> Timer::_timer;

PUBLIC
Timer::Timer()
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timer_phys_base))
{
  write<Mword>(1,          OIER); // enable OSMR0
  write<Mword>(0,          OWER); // disable Watchdog
  write<Mword>(Timer_diff, OSMR0);
  write<Mword>(0,          OSCR); // set timer counter to zero
  write<Mword>(~0U,        OSSR); // clear all status bits
}

IMPLEMENT
void Timer::init(Cpu_number)
{
  _timer.construct();
}

static inline
Unsigned64
Timer::timer_to_us(Unsigned32 cr)
{ return (((Unsigned64)cr) << 14) / 60398; }

static inline
Unsigned64
Timer::us_to_timer(Unsigned64 us)
{ return (us * 60398) >> 14; }

PUBLIC inline NEEDS["config.h", Timer::timer_to_us]
void
Timer::ack()
{
  if (Config::Scheduler_one_shot)
    {
      Kip::k()->clock += timer_to_us(read<Unsigned32>(OSCR));
      //puts("Reset timer");
      write<Mword>(0, OSCR);
      write<Mword>(0xffffffff, OSMR0);
    }
  else
    write<Mword>(0, OSCR);
  write<Mword>(1, OSSR); // clear all status bits

  // hmmm?
  //enable();
}

PUBLIC static inline
void
Timer::acknowledge()
{
  _timer->ack();
}

IMPLEMENT inline NEEDS["kip.h", Timer::timer_to_us, Timer::us_to_timer]
void
Timer::update_one_shot(Unsigned64 wakeup)
{
  Unsigned32 apic;
  Kip::k()->clock += timer_to_us(_timer->read<Unsigned32>(OSCR));
  _timer->write(0, OSCR);
  Unsigned64 now = Kip::k()->clock;

  if (EXPECT_FALSE (wakeup <= now) )
    // already expired
    apic = 1;
  else
    {
      apic = us_to_timer(wakeup - now);
      if (EXPECT_FALSE(apic > 0x0ffffffff))
	apic = 0x0ffffffff;
      if (EXPECT_FALSE (apic < 1) )
	// timeout too small
	apic = 1;
    }

  //printf("%15lld: Set Timer to %lld [%08x]\n", now, wakeup, apic);

  _timer->write<Mword>(apic, OSMR0);
  _timer->write<Mword>(1, OSSR); // clear all status bits
}

IMPLEMENT inline NEEDS["config.h", "kip.h", Timer::timer_to_us]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return Kip::k()->clock + timer_to_us(_timer->read<Unsigned32>(OSCR));
  else
    return Kip::k()->clock;
}
