INTERFACE:

#include "gic.h"
#include "gic_cpu_v2.h"
#include "global_data.h"

class Gic_v2 : public Gic_mixin<Gic_v2, Gic_cpu_v2>
{
  using Gic = Gic_mixin<Gic_v2, Gic_cpu_v2>;
  Per_cpu_array<Unsigned32> _sgi_template;

  static Global_data<Gic_v2 *> primary;

  static void _glbl_irq_handler()
  { primary->hit(nullptr); }

public:
  using Version = Gic_dist::V2;

  Gic_v2(void *cpu_base, void *dist_base, int num_override = -1)
  : Gic(dist_base, num_override, true, cpu_base)
  {
    if (!Gic_dist::Config_mxc_tzic)
      cpu_local_init(Cpu_number::boot_cpu());

    _cpu.enable();
  }

  Gic_v2(void *cpu_base, void *dist_base, Gic *master_mapping)
  : Gic(dist_base, master_mapping, cpu_base)
  {}

  void init_global_irq_handler()
  {
    primary = this;
    Gic::set_irq_handler(_glbl_irq_handler);
  }
};

//-------------------------------------------------------------------
IMPLEMENTATION:

DEFINE_GLOBAL Global_data<Gic_v2 *> Gic_v2::primary;

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
Gic_v2::redist_disable(Cpu_number)
{}

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

PUBLIC
void
Gic_v2::migrate_irqs(Cpu_number from, Cpu_number to)
{
  unsigned num = hw_nr_pins();
  Unsigned8 val_from = _sgi_template[from] >> 16;

  for (unsigned i = 0; i < num; i += 4)
    {
      Unsigned32 itarget = _dist.itarget(i);
      for (unsigned j = 0; j < 4; ++j, itarget >>= 8)
        if ((itarget & 0xff) == val_from)
          set_cpu(i + j, to);
    }
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
