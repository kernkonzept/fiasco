INTERFACE:

#include "gic_redist.h"

EXTENSION class Gic
{
  static Per_cpu<Gic_redist> _redist;

public:
  typedef Unsigned64 Sgi_target;
};

//-------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_PER_CPU Per_cpu<Gic_redist> Gic::_redist;

PUBLIC static inline
Gic::Sgi_target
Gic::pcpu_to_sgi(Cpu_phys_id cpu)
{
  return Gic_cpu::pcpu_to_sgi(cpu);
}

PUBLIC inline
void
Gic::softint_cpu(Gic::Sgi_target sgi_target, unsigned m)
{ _cpu.softint_cpu(sgi_target, m); }

PUBLIC inline
void
Gic::softint_bcast(unsigned m)
{ _cpu.softint_bcast(m); }

PRIVATE inline
void
Gic::cpu_local_init(Cpu_number cpu)
{
  _redist.cpu(cpu).find(cpu);
  _redist.cpu(cpu).cpu_init();
}

PUBLIC
void
Gic::mask_percpu(Cpu_number cpu, Mword pin)
{
  assert(pin < 32);
  assert (cpu_lock.test());
  _redist.cpu(cpu).mask(pin);
}

PUBLIC
void
Gic::unmask_percpu(Cpu_number cpu, Mword pin)
{
  assert(pin < 32);
  assert (cpu_lock.test());
  _redist.cpu(cpu).unmask(pin);
}


//-------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Gic::irq_prio_bootcpu(unsigned irq, unsigned prio)
{
  assert(irq < 32);
  _redist.cpu(Cpu_number::boot_cpu()).irq_prio(irq, prio);
}

PUBLIC
unsigned
Gic::irq_prio_bootcpu(unsigned irq)
{
  assert(irq < 32);
  return _redist.cpu(Cpu_number::boot_cpu()).irq_prio(irq);
}


