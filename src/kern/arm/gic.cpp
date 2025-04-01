INTERFACE [arm && pic_gic]:

#include "cpu.h"
#include "kmem.h"
#include "irq_chip_generic.h"
#include "gic_dist.h"

#include <cassert>
#include <cstdio>

class Gic : public Irq_chip_gen
{
  friend class Jdb;

protected:
  Gic_dist _dist;

public:
  explicit Gic(void *dist_base) : _dist(dist_base) {}

  virtual void softint_cpu(Cpu_number target, unsigned m) = 0;


  // init / pm only functions (rarely used)
  virtual void softint_bcast(unsigned m) = 0;
  virtual void softint_phys(unsigned m, Unsigned64 target) = 0;
  virtual void init_ap(Cpu_number cpu, bool resume) = 0;
  virtual void cpu_deinit(Cpu_number cpu) = 0;
  virtual unsigned gic_version() const = 0;

  // empty default for JDB
  virtual void irq_prio_bootcpu(unsigned, unsigned) {}
  virtual unsigned irq_prio_bootcpu(unsigned) { return 0; }
  virtual unsigned get_pmr() { return 0; }
  virtual void set_pmr(unsigned) {}
  virtual unsigned get_pending() { return 1023; }
};

template<typename IMPL, typename CPU>
class Gic_mixin : public Gic
{
private:
  friend class Jdb;

  using Self = IMPL;
  IMPL const *self() const { return static_cast<IMPL const *>(this); }
  IMPL *self() { return static_cast<IMPL *>(this); }

  using Cpu = CPU;

protected:
  Cpu _cpu;

public:
  template<typename ...CPU_ARGS>
  Gic_mixin(void *dist_base, int nr_irqs_override, bool dist_init, CPU_ARGS &&...args)
  : Gic(dist_base), _cpu(cxx::forward<CPU_ARGS>(args)...)
  {
    unsigned num = init(dist_init, nr_irqs_override);
    printf("GIC: Number of IRQs available at this GIC: %d\n", num);
    Irq_chip_gen::init(num);
  }

  /**
   * Create a GIC device that is a physical alias for the master GIC.
   */
  template<typename ...CPU_ARGS>
  Gic_mixin(void *dist_base, Gic *master_mapping, CPU_ARGS &&...args)
  : Gic(dist_base), _cpu(cxx::forward<CPU_ARGS>(args)...)
  {
    Irq_chip_gen::init(master_mapping->nr_irqs());
  }

  void init_ap(Cpu_number cpu, bool resume) override
  {
    if (!resume)
      self()->cpu_local_init(cpu);

    _cpu.enable();
  }

  void cpu_deinit(Cpu_number cpu) override
  {
    self()->migrate_irqs(cpu, Cpu_number::boot_cpu());
    self()->redist_disable(cpu);
    _cpu.disable();
  }

  unsigned init(bool dist_init, int nr_irqs_override = -1)
  {
    unsigned num;

    if (dist_init)
      num = _dist.init(typename IMPL::Version(), Cpu::Cpu_prio_val,
                       nr_irqs_override);
    else
      num = nr_irqs_override >= 0 ? static_cast<unsigned>(nr_irqs_override)
                                  : _dist.hw_nr_irqs();

    self()->init_global_irq_handler();

    return num;
  }

  void acknowledge_locked(unsigned irq)
  {
    if (!Gic_dist::Config_mxc_tzic)
      _cpu.ack(irq);
  }

  void mask(Mword pin) override
  {
    assert (cpu_lock.test());
    disable_locked(pin);
  }

  void mask_and_ack(Mword pin) override
  {
    assert (cpu_lock.test());
    disable_locked(pin);
    acknowledge_locked(pin);
  }

  void ack(Mword pin) override
  {
    acknowledge_locked(pin);
  }

  unsigned gic_version() const override
  { return IMPL::Version::value; }

  Unsigned32 pending()
  {
    if (Gic_dist::Config_mxc_tzic)
      return _dist.mxc_pending();

    Unsigned32 ack = _cpu.iar();

    // GICv2 only: for SGIs, bits [12:10] identify the source CPU interface.
    // For all other interrupts these bits are zero.
    Unsigned32 intid = ack & Cpu::Cpu_iar_intid_mask;

    // Ack SGIs (IPIs) immediately, the whole ack value must be used,
    // including the source CPU interface identifier.
    if (intid < 16)
      _cpu.ack(ack);

    return intid;
  }

  unsigned get_pending() override
  { return pending(); }

  void hit(Upstream_irq const *u)
  {
    Unsigned32 num = pending();

    // INTIDs 1020 - 1023 are spurious on GIC v2 and v3 and do not need an EOI
    if (EXPECT_FALSE((num & 0xfffffffc) == 0x3fc))
      return;

    handle_irq<IMPL>(num, u);
  }

  unsigned get_pmr() override { return _cpu.pmr(); }
  void set_pmr(unsigned prio) override { _cpu.pmr(prio); }
  void disable_locked(unsigned irq)
  { _dist.disable_irq(typename IMPL::Version(), irq); }

  void enable_locked(unsigned irq)
  { _dist.enable_irq(irq); }

  void unmask(Mword pin) override
  {
    assert (cpu_lock.test());
    enable_locked(pin);
  }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

#include <cstring>
#include <cstdio>

#include "io.h"
#include "irq_chip_generic.h"
#include "panic.h"
#include "processor.h"

extern "C" void irq_handler()
{ panic("Invalid IRQ handler."); }

PUBLIC inline
unsigned
Gic::hw_nr_irqs()
{ return _dist.hw_nr_irqs(); }

PUBLIC
int
Gic::set_mode(Mword pin, Mode m) override
{
  return _dist.set_mode(pin, m);
}

PUBLIC
bool
Gic::is_edge_triggered(Mword pin) const override
{
  return _dist.is_edge_triggered(pin);
}


//-------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_em_tz]:

PUBLIC
bool
Gic::attach(Irq_base *irq, Mword pin, bool init = true) override
{
  // allow local irqs to be allocated on each CPU
  return (pin < 32 && irq->chip() == this && irq->pin() == pin)
         || Irq_chip_gen::attach(irq, pin, init);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz]:

PUBLIC
bool
Gic::attach(Irq_base *irq, Mword pin, bool init = true) override
{
  if ((pin < 32 && irq->chip() == this && irq->pin() == pin)
      || Irq_chip_gen::attach(irq, pin, init))
    {
      printf("GIC: Switching IRQ %ld to secure\n", pin);
      _dist.setup_pin_grp0(pin);
      return true;
    }
  return false;
}

PUBLIC
void
Gic::set_pending_irq(unsigned idx, Unsigned32 val)
{
  _dist.set_pending_irq(idx, val);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:
PUBLIC
char const *
Gic::chip_type() const override
{ return "GIC"; }

// --------------------------------------------------------------------------
IMPLEMENTATION [32bit && !cpu_virt]:

PUBLIC static void
Gic::set_irq_handler(void (*irq_handler)())
{
  extern void (*__irq_handler_fiq)();
  extern void (*__irq_handler_irq)();
  __irq_handler_fiq = irq_handler;
  __irq_handler_irq = irq_handler;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [32bit && cpu_virt]:

PUBLIC static void
Gic::set_irq_handler(void (*irq_handler)())
{
  extern void (*__irq_handler_irq)();
  __irq_handler_irq = irq_handler;
}

// --------------------------------------------------------------------------
IMPLEMENTATION [64bit]:

#include "mem_unit.h"

PUBLIC static void
Gic::set_irq_handler(void (*irq_handler)())
{
  extern Unsigned32 __irq_handler_b_irq[1];
  auto diff = reinterpret_cast<Unsigned32 *>(irq_handler) - &__irq_handler_b_irq[0];
  // b imm26 (128 MB offset)
  __irq_handler_b_irq[0] = 0x14000000 | (diff & 0x3ffffff);
  Mem_unit::flush_cache(__irq_handler_b_irq, __irq_handler_b_irq + 1);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && cascade_irq && pf_realview]:

#include "cascade_irq.h"

PUBLIC
template<typename IMPL, typename CPU>
static void
Gic_mixin<IMPL, CPU>::cascade_hit(Irq_base *_self, Upstream_irq const *u)
{
  // this function calls some virtual functions that might be
  // ironed out
  Cascade_irq *self = nonull_static_cast<Cascade_irq*>(_self);
  IMPL *gic = nonull_static_cast<Self*>(self->child());
  Upstream_irq ui(self, u);
  gic->hit(&ui);
}
