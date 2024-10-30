// --------------------------------------------------------------------------
INTERFACE [arm && pf_tegra2 && mptimer]:

EXTENSION class Timer
{
private:
  static Mword interval() { return 249999; }
};

// --------------------------------------------------------------------------
INTERFACE [arm && pf_tegra3 && mptimer]:

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

#include "kmem_mmio.h"

Static_object<Mmio_register_block> Timer::_tmr;

IMPLEMENT
void Timer::init(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      _tmr.construct(Kmem_mmio::map(Mem_layout::Tmr_phys_base, 0x10));
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
