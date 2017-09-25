INTERFACE [arm && pic_gic]:

#include "kmem.h"
#include "irq_chip_generic.h"
#include "mmio_register_block.h"


class Gic : public Irq_chip_gen
{
private:
  Mmio_register_block _cpu;
  Mmio_register_block _dist;

public:
  enum
  {
    GICD_CTRL         = 0x000,
    GICD_TYPER        = 0x004,
    GICD_IIDR         = 0x008,
    GICD_IGROUPR      = 0x080,
    GICD_ISENABLER    = 0x100,
    GICD_ICENABLER    = 0x180,
    GICD_ISPENDR      = 0x200,
    GICD_ICPENDR      = 0x280,
    GICD_IPRIORITYR   = 0x400,
    GICD_ITARGETSR    = 0x800,
    GICD_ICFGR        = 0xc00,
    GICD_SGIR         = 0xf00,

    MXC_TZIC_PRIOMASK = 0x00c,
    MXC_TZIC_SYNCCTRL = 0x010,
    MXC_TZIC_PND      = 0xd00,

    GICC_CTRL         = 0x00,
    GICC_PMR          = 0x04,
    GICC_BPR          = 0x08,
    GICC_IAR          = 0x0c,
    GICC_EOIR         = 0x10,
    GICC_RPR          = 0x14,
    GICC_HPPIR        = 0x18,

    GICD_CTRL_ENABLE         = 1,

    MXC_TZIC_CTRL_NSEN       = 1 << 16,
    MXC_TZIC_CTRL_NSENMASK   = 1 << 31,

    GICC_CTRL_ENABLE_GRP0    = 1 << 0,
    GICC_CTRL_ENABLE_GRP1    = 1 << 1,
    GICC_CTRL_ENABLE         = GICC_CTRL_ENABLE_GRP0,
    GICC_CTRL_FIQEn          = 1 << 3,

    Cpu_prio_val      = 0xf0,
  };

};

// ------------------------------------------------------------------------
INTERFACE [arm && pic_gic && pic_gic_mxc_tzic]:

EXTENSION class Gic { enum { Config_mxc_tzic = 1 }; };

// ------------------------------------------------------------------------
INTERFACE [arm && pic_gic && !pic_gic_mxc_tzic]:

EXTENSION class Gic { enum { Config_mxc_tzic = 0 }; };

// ------------------------------------------------------------------------
INTERFACE [arm && arm_em_tz]:

EXTENSION class Gic { enum { Config_tz_sec = 1 }; };

// ------------------------------------------------------------------------
INTERFACE [arm && !arm_em_tz]:

EXTENSION class Gic { enum { Config_tz_sec = 0 }; };


//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

#include <cassert>
#include <cstring>
#include <cstdio>

#include "cascade_irq.h"
#include "io.h"
#include "irq_chip_generic.h"
#include "panic.h"
#include "processor.h"

PUBLIC inline NEEDS["io.h"]
unsigned
Gic::hw_nr_irqs()
{ return ((_dist.read<Mword>(GICD_TYPER) & 0x1f) + 1) * 32; }

PUBLIC inline NEEDS["io.h"]
bool
Gic::has_sec_ext()
{ return _dist.read<Mword>(GICD_TYPER) & (1 << 10); }

PUBLIC inline
void Gic::softint_cpu(unsigned callmap, unsigned m)
{
  _dist.write<Mword>(((callmap & 0xff) << 16) | m, GICD_SGIR);
}

PUBLIC inline
void Gic::softint_bcast(unsigned m)
{ _dist.write<Mword>((1 << 24) | m, GICD_SGIR); }

PUBLIC inline
void Gic::pmr(unsigned prio)
{ _cpu.write<Mword>(prio, GICC_PMR); }

PRIVATE
void
Gic::cpu_init(bool resume)
{
  Mword sec_irqs;

  if (Config_tz_sec)
    sec_irqs = 0x00000f00;

  _cpu.write<Mword>(0, GICC_CTRL);

  if (!resume)
    {
      // do not touch the distrubutor on cpu resume
      _dist.write<Mword>(0xffffffff, GICD_ICENABLER);
      if (Config_tz_sec)
        {
          _dist.write<Mword>(0x00000f00, GICD_ISENABLER);
          _dist.write<Mword>(~sec_irqs, GICD_IGROUPR);
        }
      else
        {
          _dist.write<Mword>(0x0000001e, GICD_ISENABLER);
          _dist.write<Mword>(0, GICD_IGROUPR);
        }

      _dist.write<Mword>(0xffffffff, GICD_ICPENDR);

      _dist.write<Mword>(0xffffffff, 0x380); // clear active
      _dist.write<Mword>(0xffffffff, 0xf10); // sgi pending clear
      _dist.write<Mword>(0xffffffff, 0xf14); // sgi pending clear
      _dist.write<Mword>(0xffffffff, 0xf18); // sgi pending clear
      _dist.write<Mword>(0xffffffff, 0xf1c); // sgi pending clear

      for (unsigned g = 0; g < 32; g += 4)
        {
          Mword v = 0;
          if (Config_tz_sec)
            {
              unsigned b = (sec_irqs >> g) & 0xf;

              for (int i = 0; i < 4; ++i)
                if (b & (1 << i))
                  v |= 0x40 << (i * 8);
                else
                  v |= 0xa0 << (i * 8);
            }
          else
            v = 0xa0a0a0a0;

          _dist.write<Mword>(v, GICD_IPRIORITYR + g);
        }
    }

  _cpu.write<Mword>(GICC_CTRL_ENABLE | (Config_tz_sec ? GICC_CTRL_FIQEn : 0),
                    GICC_CTRL);
  pmr(Cpu_prio_val);
}

PUBLIC
void
Gic::init_ap(bool resume)
{
  cpu_init(resume);
}

PUBLIC
unsigned
Gic::init(bool primary_gic, int nr_irqs_override = -1)
{
  if (!primary_gic)
    {
      cpu_init(false);
      return 0;
    }

  _dist.write<Mword>(0, GICD_CTRL);

  unsigned num = hw_nr_irqs();
  if (nr_irqs_override != -1)
    num = nr_irqs_override;

  if (!Config_mxc_tzic)
    {
      unsigned int intmask = 1U << cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
      intmask |= intmask << 8;
      intmask |= intmask << 16;

      for (unsigned i = 32; i < num; i += 16)
        _dist.write<Mword>(0, GICD_ICFGR + i * 4 / 16);
      for (unsigned i = 32; i < num; i += 4)
        _dist.write<Mword>(intmask, GICD_ITARGETSR + i);
    }

  for (unsigned i = 32; i < num; i += 4)
    _dist.write<Mword>(0xa0a0a0a0, GICD_IPRIORITYR + i);

  for (unsigned i = 32; i < num; i += 32)
    _dist.write<Mword>(0xffffffff, GICD_ICENABLER + i * 4 / 32);

  Mword v = 0;
  if (Config_tz_sec || Config_mxc_tzic)
    v = 0xffffffff;

  for (unsigned i = 32; i < num; i += 32)
    _dist.write<Mword>(v, GICD_IGROUPR + i / 8);

  Mword dist_enable = GICD_CTRL_ENABLE;
  if (Config_mxc_tzic && !Config_tz_sec)
    dist_enable |= MXC_TZIC_CTRL_NSEN | MXC_TZIC_CTRL_NSENMASK;

  for (unsigned i = 0; i < num; ++i)
    set_cpu(i, Cpu_number(0));

  _dist.write<Mword>(dist_enable, GICD_CTRL);

  if (Config_mxc_tzic)
    {
      _dist.write<Mword>(0x0, MXC_TZIC_SYNCCTRL);
      _dist.write<Mword>(Cpu_prio_val, MXC_TZIC_PRIOMASK);
    }
  else
    cpu_init(false);

  return num;
}

PUBLIC
Gic::Gic(Address cpu_base, Address dist_base, int nr_irqs_override = -1)
  : _cpu(cpu_base), _dist(dist_base)
{
  unsigned num = init(true, nr_irqs_override);

  printf("Number of IRQs available at this GIC: %d\n", num);

  Irq_chip_gen::init(num);
}

/**
 * \brief Create a GIC device that is a physical alias for the
 *        master GIC.
 */
PUBLIC inline
Gic::Gic(Address cpu_base, Address dist_base, Gic *master_mapping)
  : _cpu(cpu_base), _dist(dist_base)
{
  Irq_chip_gen::init(master_mapping->nr_irqs());
}

PUBLIC inline NEEDS["io.h"]
void Gic::disable_locked( unsigned irq )
{ _dist.write<Mword>(1 << (irq % 32), GICD_ICENABLER + (irq / 32) * 4); }

PUBLIC inline NEEDS["io.h"]
void Gic::enable_locked(unsigned irq, unsigned /*prio*/)
{ _dist.write<Mword>(1 << (irq % 32), GICD_ISENABLER + (irq / 32) * 4); }

PUBLIC inline
void Gic::acknowledge_locked(unsigned irq)
{
  if (!Config_mxc_tzic)
    _cpu.write<Mword>(irq, GICC_EOIR);
}

PUBLIC
void
Gic::mask(Mword pin)
{
  assert (cpu_lock.test());
  disable_locked(pin);
}

PUBLIC
void
Gic::mask_and_ack(Mword pin)
{
  assert (cpu_lock.test());
  disable_locked(pin);
  acknowledge_locked(pin);
}

PUBLIC
void
Gic::ack(Mword pin)
{
  acknowledge_locked(pin);
}


PUBLIC
void
Gic::unmask(Mword pin)
{
  assert (cpu_lock.test());
  enable_locked(pin, 0xa);
}

PUBLIC
int
Gic::set_mode(Mword pin, Mode m)
{
  return 0;

  if (!m.set_mode())
    return 0;

  // QEMU workaround, as QEMU has a buggy implementation of
  // GICD_ICFGR up to and including version 2.1
  if (pin < 32)
    return 0;

  if (pin < 16)
    return -L4_err::EInval;

  unsigned v = 0;
  switch (m.flow_type())
    {
    case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high:
      break;
    case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high:
      v = 2;
      break;
    default:
      return -L4_err::EInval;
    };

  unsigned shift = (pin & 15) * 2;
  _dist.modify<Mword>(v << shift, 3 << shift, GICD_ICFGR + (pin >> 4) * 4);

  return 0;
}

PUBLIC
bool
Gic::is_edge_triggered(Mword pin) const
{
  if (pin < 16)
    return false;

  Mword v = _dist.read<Mword>(GICD_ICFGR + (pin >> 4) * 4);
  return (v >> ((pin & 15) * 2)) & 2;
}

PUBLIC inline
void
Gic::hit(Upstream_irq const *u)
{
  Unsigned32 num = pending();
  if (EXPECT_FALSE(num == 0x3ff))
    return;

  handle_irq<Gic>(num, u);
}

PUBLIC static
void
Gic::cascade_hit(Irq_base *_self, Upstream_irq const *u)
{
  // this function calls some virtual functions that might be
  // ironed out
  Cascade_irq *self = nonull_static_cast<Cascade_irq*>(_self);
  Gic *gic = nonull_static_cast<Gic*>(self->child());
  Upstream_irq ui(self, u);
  gic->hit(&ui);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_em_tz]:

PUBLIC
bool
Gic::alloc(Irq_base *irq, Mword pin)
{
  // allow local irqs to be allocated on each CPU
  return (pin < 32 && irq->chip() == this && irq->pin() == pin) || Irq_chip_gen::alloc(irq, pin);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz]:

PUBLIC
bool
Gic::alloc(Irq_base *irq, Mword pin)
{
  if ((pin < 32 && irq->chip() == this && irq->pin() == pin) || Irq_chip_gen::alloc(irq, pin))
    {
      printf("GIC: Switching IRQ %ld to secure\n", pin);

      unsigned shift = (pin & 3) * 8;

      _dist.clear<Mword>(1UL << (pin & 0x1f),
                         GICD_IGROUPR + (pin & ~0x1f) / 8);

      _dist.modify<Mword>(0x40 << shift, 0xff << shift, GICD_IPRIORITYR + (pin & ~3));

      return true;
    }
  return false;
}

PUBLIC
void
Gic::set_pending_irq(unsigned idx, Unsigned32 val)
{
  if (idx < 32)
    {
      Address o = idx * 4;
      Mword v = val & _dist.read<Mword>(o + GICD_IGROUPR);
      _dist.write<Mword>(v, o + GICD_ISPENDR);
    }
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && !mp && pic_gic]:

PUBLIC
void
Gic::set_cpu(Mword, Cpu_number)
{}

PUBLIC inline NEEDS["io.h"]
Unsigned32 Gic::pending()
{
  if (Config_mxc_tzic)
    {
      Address a = MXC_TZIC_PND;
      for (unsigned g = 0; g < 128; g += 32, a += 4)
        {
          Mword v = _dist.read<Mword>(a);
          if (v)
            return g + 31 - __builtin_clz(v);
        }
      return 0;
    }

  return _cpu.read<Mword>(GICC_IAR) & 0x3ff;
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pic_gic]:

#include "cpu.h"

PUBLIC inline NEEDS["io.h"]
Unsigned32 Gic::pending()
{
  Unsigned32 ack = _cpu.read<Mword>(GICC_IAR);

  // IPIs/SGIs need to take the whole ack value
  if ((ack & 0x3ff) < 16)
    _cpu.write<Mword>(ack, GICC_EOIR);

  return ack & 0x3ff;
}

PUBLIC inline NEEDS["cpu.h"]
void
Gic::set_cpu(Mword pin, Cpu_number cpu)
{
  Mword reg = GICD_ITARGETSR + (pin & ~3);
  Mword val = _dist.read<Mword>(reg);

  int shift = (pin % 4) * 8;
  unsigned pcpu = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());
  val = (val & ~(0xf << shift)) | (1 << (pcpu + shift));

  _dist.write<Mword>(val, reg);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Gic::irq_prio(unsigned irq, unsigned prio)
{
  _dist.modify<Mword>(prio << ((irq & 3) * 8),
                      0xff << ((irq & 3) * 8),
                      GICD_IPRIORITYR + (irq >> 2) * 4);
}

PUBLIC
unsigned
Gic::irq_prio(unsigned irq)
{
  return (_dist.read<Mword>(GICD_IPRIORITYR + (irq >> 2) * 4)
          >> ((irq & 3) * 8)) & 0xff;
}

PUBLIC
unsigned
Gic::pmr()
{ return _cpu.read<Mword>(GICC_PMR); }

PUBLIC
char const *
Gic::chip_type() const
{ return "GIC"; }
