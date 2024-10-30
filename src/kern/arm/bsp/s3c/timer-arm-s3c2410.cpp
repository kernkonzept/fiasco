INTERFACE [arm && (pf_s3c2410 || pf_exynos)]:

#include "kmem_mmio.h"
#include "mmio_register_block.h"

EXTENSION class Timer : private Mmio_register_block
{
private:
  enum {
    TCFG0      = 0x00,
    TCFG1      = 0x04,
    TCON       = 0x08,
    TCNTB0     = 0x0c,
    TCMPB0     = 0x10,
    TCNTO0     = 0x14,
    TCNTB1     = 0x18,
    TCMPB1     = 0x1c,
    TCNTO1     = 0x20,
    TCNTB2     = 0x24,
    TCMPB2     = 0x28,
    TCNTO2     = 0x2c,
    TCNTB3     = 0x30,
    TCMPB3     = 0x34,
    TCNTO3     = 0x38,
    TCNTB4     = 0x3c,
    TCNTO4     = 0x40,
    TINT_CSTAT = 0x44,
  };

  enum {
    Timer_nr = 4,
  };

  static Static_object<Timer> _timer;
};

INTERFACE [arm && pf_s3c2410]: // --------------------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 10 + Timer_nr; }
  enum { Reload_value = 33333, Tint_cstat_entable = 0 };
};

INTERFACE [arm && pf_exynos]: // --------------------------------------------

EXTENSION class Timer
{
public:
  static unsigned irq() { return 68 + Timer_nr; }
  enum { Reload_value = 66666, Tint_cstat_entable = 1 };
};

// -----------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_s3c2410 || pf_exynos)]:

#include "io.h"

#include <cstdio>

Static_object<Timer> Timer::_timer;

PUBLIC static
void
Timer::configure(Cpu_number)
{}

PUBLIC
Timer::Timer() : Mmio_register_block(Kmem_mmio::map(Mem_layout::Pwm_phys_base,
                                                    0x100))
{
  write<Mword>(0, TCFG0); // prescaler config
  write<Mword>(0, TCFG1); // mux select
  write<Mword>(Reload_value, TCNTB0  + Timer_nr * 0xc); // reload value
  write<Mword>(Reload_value, TCMPB0  + Timer_nr * 0xc); // reload value

  unsigned shift = Timer_nr == 0 ? 0 : (Timer_nr * 4 + 4);
  write<Mword>(5 << shift, TCON); // start + autoreload

  if (Tint_cstat_entable)
    write<Mword>(1 << Timer_nr, TINT_CSTAT);
}

IMPLEMENT
void Timer::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    _timer.construct();
}

PUBLIC static inline
void
Timer::acknowledge()
{
  if (Tint_cstat_entable)
    _timer->modify<Mword>(1 << (Timer_nr + 5), 0, TINT_CSTAT);
}
