// --------------------------------------------------------------------------
INTERFACE [arm && mptimer]:

#include "kmem.h"
#include "cpu.h"

EXTENSION class Timer
{
public:
  static unsigned irq() { return 29; }

private:
  enum
  {
    Timer_load_reg     = 0x600 + 0x0,
    Timer_counter_reg  = 0x600 + 0x4,
    Timer_control_reg  = 0x600 + 0x8,
    Timer_int_stat_reg = 0x600 + 0xc,

    Prescaler = 0,

    Timer_control_enable    = 1 << 0,
    Timer_control_reload    = 1 << 1,
    Timer_control_itenable  = 1 << 2,
    Timer_control_prescaler = (Prescaler & 0xff) << 8,

    Timer_int_stat_event   = 1,
  };
};

// --------------------------------------------------------------
IMPLEMENTATION [arm && mptimer]:

IMPLEMENT_OVERRIDE
Irq_chip::Mode Timer::irq_mode()
{
  return Irq_chip::Mode::F_level_high;
}

PRIVATE static
Mword
Timer::start_as_counter()
{
  static_assert(Scu::Available, "No SCU available in this configuration");

  Mword v = ~0UL;
  Cpu::scu->write<Mword>(v, Timer_counter_reg);

  Cpu::scu->write<Mword>(Timer_control_prescaler | Timer_control_reload
                         | Timer_control_enable,
                         Timer_control_reg);
  return v;
}

PRIVATE static
Mword
Timer::stop_counter()
{
  Mword v = Cpu::scu->read<Mword>(Timer_counter_reg);
  Cpu::scu->write<Mword>(0, Timer_control_reg);
  return v;
}

IMPLEMENT
void
Timer::init(Cpu_number)
{

  Mword i = interval();

  Cpu::scu->write<Mword>(i, Timer_load_reg);
  Cpu::scu->write<Mword>(i, Timer_counter_reg);
  Cpu::scu->write<Mword>(Timer_control_prescaler | Timer_control_reload
                         | Timer_control_enable | Timer_control_itenable,
                         Timer_control_reg);
}

PUBLIC static inline
void
Timer::acknowledge()
{
  Cpu::scu->write<Mword>(Timer_int_stat_event, Timer_int_stat_reg);
}
