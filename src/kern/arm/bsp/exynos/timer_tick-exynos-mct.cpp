INTERFACE [arm && exynos_mct]: // -----------------------------------------

#include "types.h"
#include "timer.h"

EXTENSION class Timer_tick
{
private:
  Timer *_timer;

public:
  static Per_cpu<Static_object<Timer_tick> > _timer_ticks;
};

IMPLEMENTATION [arm && exynos_mct]: // ------------------------------------

#include "pic.h"
#include "platform.h"

DEFINE_PER_CPU Per_cpu<Static_object<Timer_tick> > Timer_tick::_timer_ticks;

IMPLEMENTATION [arm && exynos_mct && exynos_extgic]: // -------------------

PRIVATE static
bool
Timer_tick::alloc_irq_4412_timer_tick(Cpu_number cpu, unsigned irq)
{
  return Pic::gic.cpu(cpu)->alloc(_timer_ticks.cpu(cpu), irq);
}

IMPLEMENTATION [arm && exynos_mct && !exynos_extgic]: // ------------------

PRIVATE static
bool
Timer_tick::alloc_irq_4412_timer_tick(Cpu_number cpu, unsigned irq)
{
  (void)cpu;
  return allocate_irq(_timer_ticks.cpu(Cpu_number::boot_cpu()), irq);
}

IMPLEMENTATION [arm && arm_generic_timer]: // -----------------------------

IMPLEMENT void Timer_tick::setup(Cpu_number cpu) {}
IMPLEMENT void Timer_tick::enable(Cpu_number cpu) {}
IMPLEMENT void Timer_tick::disable(Cpu_number cpu) {}
PUBLIC inline void Timer_tick::ack() {}


IMPLEMENTATION [arm && exynos_mct]: // ------------------------------------

IMPLEMENT
void
Timer_tick::setup(Cpu_number cpu)
{
  int irq;
  bool r;
  unsigned pcpu = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());

  _timer_ticks.cpu(cpu).construct(cpu == Cpu_number::boot_cpu()
                                  ? Sys_cpu : App_cpu);

  if (Platform::is_5250() || Platform::is_5410())
    {
      assert(cpu < Cpu_number(Platform::is_5410() ? 4 : 2));
      irq = 152 + pcpu;
      r = allocate_irq(_timer_ticks.cpu(cpu), irq);
    }
  else if (Platform::is_4412())
    {
      assert(cpu < Cpu_number(4));
      irq = 28;
      r = alloc_irq_4412_timer_tick(cpu, irq);
    }
  else
    {
      assert(cpu < Cpu_number(2));

      if (Platform::gic_int())
        irq = pcpu == 0 ? 96 + 51 * 8 + 0 : 96 + 35 * 8 + 3;
      else
        irq = pcpu == 0 ? 74 : 80;

      r = allocate_irq(_timer_ticks.cpu(cpu), irq);
    }

  if (!r)
    panic("Could not allocate scheduling IRQ %d for CPU%d\n", irq, cxx::int_value<Cpu_number>(cpu));
  else
    printf("Timer for CPU%d is at IRQ %d\n", cxx::int_value<Cpu_number>(cpu), irq);

  if (!Platform::is_4412())
    Irq_mgr::mgr->set_cpu(irq, cpu);

  _timer_ticks.cpu(cpu)->_timer = Timer::timers.cpu(cpu).get();
}

IMPLEMENT
void
Timer_tick::enable(Cpu_number cpu)
{
  Timer_tick &t = *_timer_ticks.cpu(cpu).get();
  t.chip()->unmask(t.pin());
}

IMPLEMENT
void
Timer_tick::disable(Cpu_number cpu)
{
  Timer_tick &t = *_timer_ticks.cpu(cpu).get();
  t.chip()->mask(t.pin());
}

PUBLIC inline
void
Timer_tick::ack()
{
  _timer->acknowledge();
  Irq_base::ack();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug && arm && exynos_mct]:

IMPLEMENT
Timer_tick *
Timer_tick::boot_cpu_timer_tick()
{ return _timer_ticks.cpu(Cpu_number::boot_cpu()); }
