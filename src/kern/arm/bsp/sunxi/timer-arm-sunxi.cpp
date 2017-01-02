INTERFACE [arm && pf_sunxi]:

#include "config.h"
#include "kmem.h"
#include "mmio_register_block.h"

EXTENSION class Timer
{
public:
  static unsigned irq() { return 54; }

  enum { Interval = 24000000 / Config::Scheduler_granularity };

private:
  class Tmr : public Mmio_register_block
  {
  public:
    enum {
      TMR_IRQ_EN_REG      = 0x0,
      TMR_IRQ_STA_REG     = 0x4,

      TMRx_base           = 0x10,
      TMRx_offset         = 0x10,
      TMRx_CTRL_REG       = 0x0,
      TMRx_INTV_VALUE_REG = 0x4,
      TMRx_CUR_VALUE_REG  = 0x8,

      Timer_nr = 0,
    };

    Tmr(Address a) : Mmio_register_block(a) {}

    static Address addr(unsigned reg)
    { return TMRx_base + Timer_nr * TMRx_offset + reg; }
  };

  private:
    static Static_object<Tmr> tmr;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_sunxi]:

Static_object<Timer::Tmr> Timer::tmr;

IMPLEMENT
void Timer::init(Cpu_number)
{
  tmr.construct(Kmem::mmio_remap(Mem_layout::Timer_phys_base));

  tmr->write<Mword>(Interval, Tmr::addr(Tmr::TMRx_INTV_VALUE_REG));
  tmr->write<Mword>(1 | (1 << 1) | (1 << 2), Tmr::addr(Tmr::TMRx_CTRL_REG));

  tmr->write<Mword>(1 << Tmr::Timer_nr, Tmr::TMR_IRQ_EN_REG);
}

PUBLIC static
void
Timer::acknowledge()
{
  tmr->write<Mword>(1 << Tmr::Timer_nr, Tmr::TMR_IRQ_STA_REG);
}

IMPLEMENT inline
void
Timer::update_one_shot(Unsigned64)
{}

IMPLEMENT inline NEEDS["config.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  return Kip::k()->clock;
}
