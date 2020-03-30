INTERFACE:

#include "gic.h"
#include "gic_cpu_v2.h"

class Gic_v2 : public Gic_mixin<Gic_v2, Gic_cpu_v2>
{
  using Gic = Gic_mixin<Gic_v2, Gic_cpu_v2>;
  Per_cpu_array<Unsigned32> _sgi_template;

public:
  using Version = Gic_dist::V2;

  Gic_v2(Address cpu_base, Address dist_base, int num_override = -1)
  : Gic(dist_base, num_override, cpu_base)
  {
    if (!Gic_dist::Config_mxc_tzic)
      cpu_local_init(Cpu_number::boot_cpu());

    _cpu.enable();
  }

  Gic_v2(Address cpu_base, Address dist_base, Gic *master_mapping)
  : Gic(dist_base, master_mapping, cpu_base)
  {}
};

//-------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
void
Gic_v2::softint_cpu(Cpu_number target, unsigned m) override
{ _dist.softint(_sgi_template[target] | m); }

PUBLIC inline
void
Gic_v2::softint_bcast(unsigned m) override
{ _dist.softint((1u << 24) | m); }

PUBLIC inline
void
Gic_v2::softint_phys(unsigned m, Unsigned64 target) override
{ _dist.softint(target | m); }

PUBLIC inline
void
Gic_v2::cpu_local_init(Cpu_number cpu)
{
  _dist.cpu_init_v2();
  // initialize our CPU target using the target register from some
  // CPU local IRQ
  _sgi_template[cpu] = _dist.itarget(0) & 0x00ff0000;
}

PUBLIC inline
void
Gic_v2::set_cpu(Mword pin, Cpu_number cpu) override
{
  _dist.set_cpu(pin, _sgi_template[cpu] >> 16, Version());
}

//-------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Gic_v2::irq_prio_bootcpu(unsigned irq, unsigned prio) override
{
  _dist.irq_prio(irq, prio);
}

PUBLIC
unsigned
Gic_v2::irq_prio_bootcpu(unsigned irq) override
{
  return _dist.irq_prio(irq);
}
