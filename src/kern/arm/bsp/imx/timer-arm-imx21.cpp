// --------------------------------------------------------------------------
INTERFACE [arm && pf_imx_21]:

#include "kmem.h"
#include "irq_chip.h"
#include "mmio_register_block.h"


EXTENSION class Timer
{
public:
  static unsigned irq() { return 26; }

private:
  class Timer_imx21 : private Mmio_register_block
  {
  public:
    Timer_imx21();

  private:
    enum {
      TCTL  = 0x00,
      TPRER = 0x04,
      TCMP  = 0x08,
      TCR   = 0x0c,
      TCN   = 0x10,
      TSTAT = 0x14,

      TCTL_TEN                            = 1 << 0,
      TCTL_CLKSOURCE_PERCLK1_TO_PRESCALER = 1 << 1,
      TCTL_CLKSOURCE_32kHz                = 1 << 3,
      TCTL_COMP_EN                        = 1 << 4,
      TCTL_SW_RESET                       = 1 << 15,
    };

  public:
    void acknowledge() const { write<Mword>(1, TSTAT); }
  };

  static Static_object<Timer_imx21> _timer;
};

// ----------------------------------------------------------------------
IMPLEMENTATION [arm && pf_imx_21]:

#include "config.h"
#include "kip.h"
#include "io.h"

#include <cstdio>

IMPLEMENT
Timer::Timer_imx21::Timer_imx21()
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timer_phys_base))
{
  write<Mword>(0, TCTL); // Disable
  write<Mword>(TCTL_SW_RESET, TCTL); // reset timer
  for (int i = 0; i < 10; ++i)
    read<Mword>(TCN); // docu says reset takes 5 cycles

  write<Mword>(TCTL_CLKSOURCE_32kHz | TCTL_COMP_EN, TCTL);
  write<Mword>(0, TPRER);
  write<Mword>(32, TCMP);

  modify<Mword>(TCTL_TEN, 0, TCTL);
}

Static_object<Timer::Timer_imx21> Timer::_timer;

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
    //return Kip::k()->clock + timer_to_us(Io::read<Unsigned32>(OSCR));
    return 0;
  else
    return Kip::k()->clock;
}
