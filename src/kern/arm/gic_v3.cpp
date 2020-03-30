INTERFACE:

#include "gic.h"
#include "gic_redist.h"
#include "gic_cpu_v3.h"

class Gic_v3 : public Gic_mixin<Gic_v3, Gic_cpu_v3>
{
  using Gic = Gic_mixin<Gic_v3, Gic_cpu_v3>;

  static Per_cpu<Gic_redist> _redist;
  Per_cpu_array<Unsigned64> _sgi_template;

  Address _redist_base;

public:
  using Version = Gic_dist::V3;

  explicit Gic_v3(Address dist_base, Address redist_base)
  : Gic(dist_base, -1), _redist_base(redist_base)
  {
    cpu_local_init(Cpu_number::boot_cpu());
    _cpu.enable();
  }
};

//-------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_PER_CPU Per_cpu<Gic_redist> Gic_v3::_redist;

PUBLIC inline
void
Gic_v3::softint_cpu(Cpu_number target, unsigned m) override
{
  Unsigned64 sgi = _sgi_template[target] | (m << 24);
  _cpu.softint(sgi);
}

PUBLIC inline
void
Gic_v3::softint_bcast(unsigned m) override
{ _cpu.softint((1ull << 40) | (m << 24)); }

PUBLIC inline
void
Gic_v3::softint_phys(unsigned m, Unsigned64 target) override
{ _cpu.softint(target | (m << 24)); }

PUBLIC inline
void
Gic_v3::cpu_local_init(Cpu_number cpu)
{
  auto &rd = _redist.cpu(cpu);
  Unsigned64 mpidr = ::Cpu::mpidr();

  rd.find(_redist_base, mpidr, cpu);
  rd.cpu_init();

  if (mpidr & 0xf0)
    {
      _sgi_template[cpu] = ~0ull;
      printf("GICv3: Cpu%u affinity level 0 out of range: %u max is 15\n",
             cxx::int_value<Cpu_number>(cpu), (unsigned)(mpidr % 0xff));
      return;
    }

  _sgi_template[cpu] = (1u << (mpidr & 0xf))
                       | ((mpidr & (Unsigned64)0xff00) << 8)
                       | ((mpidr & (Unsigned64)0xff00ff0000) << 16);
}

PUBLIC
void
Gic_v3::set_cpu(Mword pin, Cpu_number cpu) override
{
  _dist.set_cpu(pin, ::Cpu::cpus.cpu(cpu).phys_id(), Version());
}

PUBLIC
void
Gic_v3::mask_percpu(Cpu_number cpu, Mword pin) override
{
  assert(pin < 32);
  assert (cpu_lock.test());
  _redist.cpu(cpu).mask(pin);
}

PUBLIC
void
Gic_v3::unmask_percpu(Cpu_number cpu, Mword pin) override
{
  assert(pin < 32);
  assert (cpu_lock.test());
  _redist.cpu(cpu).unmask(pin);
}


//-------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Gic_v3::irq_prio_bootcpu(unsigned irq, unsigned prio) override
{
  assert(irq < 32);
  _redist.cpu(Cpu_number::boot_cpu()).irq_prio(irq, prio);
}

PUBLIC
unsigned
Gic_v3::irq_prio_bootcpu(unsigned irq) override
{
  assert(irq < 32);
  return _redist.cpu(Cpu_number::boot_cpu()).irq_prio(irq);
}


