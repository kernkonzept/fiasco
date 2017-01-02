// --------------------------------------------------------------------------
INTERFACE [arm && pf_imx_28]:

#include "kmem.h"
#include "irq_chip.h"
#include "mmio_register_block.h"

EXTENSION class Timer
{
public:
  static unsigned irq() { return 48; }

private:
  class Timer_imx_timrot
  {
  public:
    Timer_imx_timrot();

  private:
    enum
    {
      HW_TIMROT_TIMCTRL0       = 0x20,
      HW_TIMROT_TIMCTRL0_SET   = 0x24,
      HW_TIMROT_TIMCTRL0_CLR   = 0x28,
      HW_TIMROT_RUNNING_COUNT0 = 0x30,
      HW_TIMROT_FIXED_COUNT0   = 0x40,

      CTRL_SELECT_32KHZ = 0xb << 0,
      CTRL_RELOAD       = 1   << 6,
      CTRL_UPDATE       = 1   << 7,
      CTRL_IRQ_EN       = 1   << 14,
      CTRL_IRQ          = 1   << 15,
    };

    Register_block<32> _reg;

  public:
    void acknowledge() const { _reg[HW_TIMROT_TIMCTRL0_CLR] = CTRL_IRQ; }
  };

  static Static_object<Timer_imx_timrot> _timer;
};

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_28]:

#include "config.h"
#include "kip.h"
#include "io.h"

#include <cstdio>

IMPLEMENT
Timer::Timer_imx_timrot::Timer_imx_timrot()
: _reg(Kmem::mmio_remap(Mem_layout::Timer_phys_base))
{
  _reg[HW_TIMROT_TIMCTRL0] = 0;
  _reg[HW_TIMROT_TIMCTRL0_SET]
    = CTRL_SELECT_32KHZ | CTRL_RELOAD | CTRL_UPDATE | CTRL_IRQ_EN;

  Unsigned32 v = 32000 / (1000000 / Config::Scheduler_granularity);
  _reg[HW_TIMROT_FIXED_COUNT0] = v;
}

Static_object<Timer::Timer_imx_timrot> Timer::_timer;

IMPLEMENT
void Timer::init(Cpu_number)
{
  _timer.construct();
}

PUBLIC static inline NEEDS["io.h"]
void
Timer::acknowledge()
{
  _timer->acknowledge();
}

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{
}

IMPLEMENT inline NEEDS["config.h", "kip.h", "io.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  return Kip::k()->clock;
}
