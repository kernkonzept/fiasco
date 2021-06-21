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

  explicit Gic_v3(Address dist_base, Address redist_base, Address its_base = 0)
  : Gic(dist_base, -1), _redist_base(redist_base)
  {
    init_lpi(its_base);

    cpu_local_init(Cpu_number::boot_cpu());
    _cpu.enable();
  }
};

//-------------------------------------------------------------------
INTERFACE [arm_gic_msi]:

#include "gic_its.h"
#include "gic_msi.h"

EXTENSION class Gic_v3
{
private:
  enum
  {
    // Limit the number of supported LPIs to avoid excessive memory
    // allocations.
    Max_num_lpis = 512,
  };

  bool _has_lpis = false;
  Gic_its *_its = nullptr;
  Gic_msi *_msi = nullptr;
};

//-------------------------------------------------------------------
INTERFACE [have_arm_gic_msi && !arm_gic_msi]:

class Gic_msi;

EXTENSION class Gic_v3
{
public:
  Gic_msi *msi_chip() { return nullptr; };
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

  cpu_local_init_lpi(cpu);
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

PUBLIC
int
Gic_v3::set_mode_percpu(Cpu_number cpu, Mword pin, Mode m) override
{
  assert(pin < 32);
  assert (cpu_lock.test());
  return _redist.cpu(cpu).set_mode(pin, m);
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

//-------------------------------------------------------------------
IMPLEMENTATION[arm_gic_msi]:

PRIVATE
void
Gic_v3::init_lpi(Address its_base)
{
  unsigned hw_num_lpis = _dist.hw_nr_lpis();
  _has_lpis = hw_num_lpis > 0;
  if (_has_lpis)
    {
      unsigned num_lpis = min<unsigned>(hw_num_lpis, Max_num_lpis);

      Gic_redist::init_lpi(num_lpis);
      _its = new Boot_object<Gic_its>();
      _its->init(&_cpu, its_base, num_lpis);
      _msi = new Boot_object<Gic_msi>();
      _msi->init(_its, num_lpis);
      printf("GIC: Supports up to %u LPIs, using %u.\n", hw_num_lpis, num_lpis);
    }
  else
    WARN("GIC: Does not implement LPIs...\n");
}

PRIVATE
void
Gic_v3::cpu_local_init_lpi(Cpu_number cpu)
{
  if (_has_lpis)
  {
    _redist.cpu(cpu).cpu_init_lpi();
    _its->cpu_init(cpu, _redist.cpu(cpu));
  }
}

/**
 * \return The MSI Irq_chip for this GIC, might be nullptr if the GIC does not
 *         support LPIs or MSI support has been disabled in the Kconfig.
 */
PUBLIC inline
Gic_msi *
Gic_v3::msi_chip()
{
  return _msi;
}

PUBLIC
Irq_base *
Gic_v3::irq(Mword pin) const override
{
  if (_has_lpis && pin >= Gic_dist::Lpi_intid_base)
    return _msi->Gic_msi::irq(pin - Gic_dist::Lpi_intid_base);

  return this->Gic::irq(pin);
}

//-------------------------------------------------------------------
IMPLEMENTATION[!arm_gic_msi]:

PROTECTED inline
void
Gic_v3::init_lpi(Address)
{}

PROTECTED inline
void
Gic_v3::cpu_local_init_lpi(Cpu_number)
{}
