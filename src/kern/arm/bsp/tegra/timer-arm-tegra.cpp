// --------------------------------------------------------------------------
INTERFACE [arm && tegra2 && mptimer]:

EXTENSION class Timer
{
private:
  static Mword interval() { return 249999; }
};

// --------------------------------------------------------------------------
INTERFACE [arm && tegra3 && mptimer]:

EXTENSION class Timer
{
private:
  static Mword interval() { return 499999; }
};

// --------------------------------------------------------------------------
INTERFACE [arm && tegra_timer_tmr]:

#include "mmio_register_block.h"

EXTENSION class Timer
{
public:
  static unsigned irq() { return 32; };

private:
  static Static_object<Mmio_register_block> _tmr;

  struct Reg { enum
  {
    PTV = 0,
    PCR = 4,
  }; };
};

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && tegra_timer_tmr]:

#include "kmem.h"

Static_object<Mmio_register_block> Timer::_tmr;

IMPLEMENT
void Timer::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      _tmr.construct(Kmem::mmio_remap(Mem_layout::Tmr_phys_base));
      _tmr->write<Mword>(  (1 << 31) // enable
                         | (1 << 30) // periodic
                         | (Config::Scheduler_granularity & 0x1fffffff),
                         Reg::PTV);
    }
}


PUBLIC static inline
void
Timer::acknowledge()
{
  _tmr->write<Mword>(1 << 30, Reg::PCR);
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
