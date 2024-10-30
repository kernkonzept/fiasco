INTERFACE:

#include "gic.h"
#include "initcalls.h"

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_exynos]:

class Mgr_exynos : public Irq_mgr
{
protected:
  struct Chip_block
  {
    unsigned sz;
    Irq_chip_gen *chip;
  };

  void calc_nr_irq()
  {
    _nr_irqs = 0;
    for (unsigned i = 0; i < _nr_blocks; ++i)
      _nr_irqs += _block[i].sz;
  }

public:
  unsigned nr_irqs() const override { return _nr_irqs; }
  unsigned nr_msis() const override { return 0; }

protected:
  unsigned _nr_blocks;
  unsigned _nr_irqs;
  Chip_block *_block;
};


//-------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_exynos]:

#include <cstring>
#include <cstdio>

#include "assert.h"
#include "boot_alloc.h"
#include "config.h"
#include "initcalls.h"
#include "io.h"
#include "irq_chip.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "gic.h"
#include "kmem.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"
#include "warn.h"

#include "platform.h"


class Gpio_eint_chip : public Irq_chip_gen, private Mmio_register_block
{
private:
  void *offs(Mword reg) const
  { return offset_cast<void *>(_gpio_base, reg); }

  void *offs(Mword reg, Mword pin) const
  { return offset_cast<void *>(_gpio_base, reg + (pin >> 3) * 4); }

public:
  Gpio_eint_chip(void* gpio_base, unsigned num_irqs)
    : Irq_chip_gen(num_irqs), _gpio_base(gpio_base)
  {}

  void mask(Mword pin) override
  { Io::set<Mword>(1 << (pin & 7), offs(MASK, pin)); }

  void ack(Mword pin) override
  { Io::set<Mword>(1 << (pin & 7), offs(PEND, pin)); }

  void mask_and_ack(Mword pin) override { mask(pin); ack(pin); }

  void unmask(Mword pin) override
  { Io::clear<Mword>(1 << (pin & 7), offs(MASK, pin)); }

  void set_cpu(Mword, Cpu_number) override {}
  int set_mode(Mword pin, Mode m) override
  {
    unsigned v;

    if (m.wakeup())
      return -L4_err::EInval;

    if (!m.set_mode())
      return 0;

    switch (m.flow_type())
    {
      default:
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_low:  v = 0; break;
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high: v = 1; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_low:  v = 2; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high: v = 3; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_both: v = 4; break;
    };

    void *a = offs(INTCON, pin);
    pin = pin % 8;
    v <<= pin * 4;
    Io::write<Mword>((Io::read<Mword>(a) & ~(7 << (pin * 4))) | v, a);

    return 0;
  }

  bool is_edge_triggered(Mword pin) const override
  {
    unsigned v;
    void *a = offs(INTCON, pin);
    pin = pin % 8;
    v = (Io::read<Mword>(a) >> (pin * 4)) & 7;
    return v & 6;
  }

  unsigned pending() { return Io::read<Mword>(offs(0xb08)); }

private:
  enum {
    INTCON = 0x700,
    MASK   = 0x900,
    PEND   = 0xa00,
  };
  void *_gpio_base;
};

class Gpio_wakeup_chip : public Irq_chip_gen, private Mmio_register_block
{
private:
  unsigned offs(Mword pin) const { return (pin >> 3) * 4; }

public:
  explicit Gpio_wakeup_chip(Address physbase)
  : Irq_chip_gen(32),
    Mmio_register_block(Kmem_mmio::map(physbase, 0x1000)),
    _wakeup(0)
  {}

  void mask(Mword pin) override
  { modify<Mword>(1 << (pin & 7), 0, MASK + offs(pin)); }

  void ack(Mword pin) override
  { modify<Mword>(1 << (pin & 7), 0, PEND + offs(pin)); }

  void mask_and_ack(Mword pin) override { mask(pin); ack(pin); }

  void unmask(Mword pin) override
  { modify<Mword>(0, 1 << (pin & 7), MASK + offs(pin)); }
  void set_cpu(Mword, Cpu_number) override {}

  int set_mode(Mword pin, Mode m) override
  {
    unsigned v;

    if (m.set_wakeup() && m.clear_wakeup())
      return -L4_err::EInval;

    if (m.set_wakeup())
      _wakeup |= 1 << pin;
    else if (m.clear_wakeup())
      _wakeup &= ~(1 << pin);

    if (!m.set_mode())
      return 0;

    switch (m.flow_type())
    {
      default:
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_low:  v = 0; break;
      case Irq_chip::Mode::Trigger_level | Irq_chip::Mode::Polarity_high: v = 1; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_low:  v = 2; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_high: v = 3; break;
      case Irq_chip::Mode::Trigger_edge  | Irq_chip::Mode::Polarity_both: v = 4; break;
    };

    Mword a = INTCON + offs(pin);
    pin = pin % 8;
    v <<= pin * 4;
    modify<Mword>(v, 7UL << (pin * 4), a);

    return 0;
  }

  bool is_edge_triggered(Mword pin) const override
  {
    unsigned v;
    Mword a = INTCON + offs(pin);
    pin = pin % 8;
    v = (read<Mword>(a) >> (pin * 4)) & 7;
    return v & 6;
  }

  unsigned pending01() const // debug only
  {
    return read<Unsigned8>(PEND + 0) | (static_cast<unsigned>(read<Unsigned8>(PEND +  4)) << 8);
  }

  unsigned pending23() const
  {
    return read<Unsigned8>(PEND + 8) | (static_cast<unsigned>(read<Unsigned8>(PEND + 12)) << 8);
  }

  Mword mask23()
  { return read<Mword>(MASK + offs(16)) | (read<Mword>(MASK + offs(24)) << 8); }

  Unsigned32 _wakeup;

private:
  enum {
    INTCON = 0xe00,
    FLTCON = 0xe80,
    MASK   = 0xf00,
    PEND   = 0xf40,
  };
};

class Combiner_chip : public Irq_chip_gen, private Mmio_register_block
{
public:
  enum
  {
    Enable_set    = 0,
    Enable_clear  = 4,
    Status        = 12,
  };

  enum
  {
    No_pending = ~0UL,
  };

  Mword offset(unsigned irq) const { return (irq >> 2) * 0x10; }

  static unsigned shift(int irq)
  { return (irq % 4) * 8; }

  static Mword bytemask(int irq)
  { return 0xffUL << shift(irq); }

  Mword status(int irq) const
  { return read<Mword>(offset(irq) + Status) & bytemask(irq); }

  void mask(Mword i) override
  { write<Mword>(1UL << (i & 31), offset(i / 8) + Enable_clear); }

  void mask_and_ack(Mword i) override
  { Combiner_chip::mask(i); }

  void ack(Mword) override {}

  void set_cpu(Mword, Cpu_number) override {}

  int set_mode(Mword, Mode) override
  { return 0; }

  bool is_edge_triggered(Mword) const override
  { return false; }

  void unmask(Mword i) override
  { write<Mword>(1UL << (i & 31), offset(i / 8) + Enable_set); }

  void init_irq(int irq) const
  { write<Mword>(bytemask(irq), offset(irq) + Enable_clear); }

  Unsigned32 pending(unsigned cnr)
  {
    unsigned v = status(cnr) >> shift(cnr);
    if (v)
      return (cnr * 8) + __builtin_ctz(v);
    return No_pending;
  }

  int num_combiner_chips() const
  {
    if (Platform::is_4210())
      return Platform::gic_int() ? 54 : 16;
    if (Platform::is_4412())
      return 20;
    if (Platform::is_5250() || Platform::is_5410())
      return 32;
    assert(0);
    return 0;
  }

  Combiner_chip()
  : Irq_chip_gen(num_combiner_chips() * 8),
    Mmio_register_block(Kmem_mmio::map(Mem_layout::Irq_combiner_phys_base,
                                       0x1000))
  {
    // 0..39, 51, 53
    if (Platform::gic_int())
      {
        for (int i = 0; i < 40; ++i)
          init_irq(i);
        init_irq(51);
        init_irq(53);
      }
    else
      {
        const int num = num_combiner_chips();
        for (int i = 0; i < num; ++i)
          init_irq(i);
      }
  }
};

// ------------

class Gpio_cascade_wu01_irq : public Irq_base
{
public:
  explicit Gpio_cascade_wu01_irq(Gpio_wakeup_chip *gc, unsigned pin)
  : _wu_gpio(gc), _pin(pin)
  { set_hit(&handler_wrapper<Gpio_cascade_wu01_irq>); }

  void switch_mode(bool) override {}

  void handle(Upstream_irq const *u)
  {
    // checking pending reg as a debug thing
    if (!(_wu_gpio->pending01() & (1 << _pin)))
      WARN("WU-GPIO not pending %d\n", _pin);

    Upstream_irq ui(this, u);
    _wu_gpio->irq(_pin)->hit(&ui);
  }

private:
  Gpio_wakeup_chip *_wu_gpio;
  unsigned _pin;
};


class Gpio_cascade_wu23_irq : public Irq_base
{
public:
  explicit Gpio_cascade_wu23_irq(Gpio_wakeup_chip *gc)
  : _wu_gpio(gc)
  { set_hit(&handler_wrapper<Gpio_cascade_wu23_irq>); }

  void switch_mode(bool) override {}

private:
  Gpio_wakeup_chip *_wu_gpio;
};

PUBLIC
void
Gpio_cascade_wu23_irq::handle(Upstream_irq const *u)
{
  Unsigned32 pending = (_wu_gpio->pending23() & ~_wu_gpio->mask23()) << 16;
  Upstream_irq ui(this, u);
  while (pending)
    {
      unsigned p = __builtin_ctz(pending);
      _wu_gpio->irq(p)->hit(&ui);
      pending &= ~(1 << p);
    }
}

class Gpio_cascade_xab_irq : public Irq_base
{
public:
  explicit Gpio_cascade_xab_irq(Gpio_eint_chip *g, unsigned special = 0)
  : _eint_gc(g), _special(special)
  { set_hit(&handler_wrapper<Gpio_cascade_xab_irq>); }

  void switch_mode(bool) override {}

private:
  Gpio_eint_chip *_eint_gc;
  unsigned _special;
};

PUBLIC
void
Gpio_cascade_xab_irq::handle(Upstream_irq const *u)
{
  Mword p = _eint_gc->pending();
  Upstream_irq ui(this, u);
  if (1)
    {
      int grp = (p >> 3) & 0x1f;
      int pin = p & 7;

      if (_special == 1)
        {
          if (grp > 7)
            grp += 5;
        }
      else if (_special == 2)
        grp += 2;

      _eint_gc->irq((grp - 1) * 8 + pin)->hit(&ui);
    }
  else
    _eint_gc->irq(p - 8)->hit(&ui);
}

// ------------
class Combiner_cascade_irq : public Irq_base
{
public:
  Combiner_cascade_irq(unsigned nr, Combiner_chip *chld)
  : _combiner_nr(nr), _child(chld)
  { set_hit(&handler_wrapper<Combiner_cascade_irq>); }

  void switch_mode(bool) override {}
  unsigned irq_nr_base() const { return _combiner_nr * 8; }

private:
  unsigned _combiner_nr;
  Combiner_chip *_child;
};

PUBLIC
void
Combiner_cascade_irq::handle(Upstream_irq const *u)
{
  Unsigned32 num = _child->pending(_combiner_nr);
  Upstream_irq ui(this, u);

  if (num != Combiner_chip::No_pending)
    _child->irq(num)->hit(&ui);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [pf_exynos4 && !exynos_extgic]:

#include "gic_v2.h"

class Mgr_int : public Mgr_exynos
{
public:

  Irq chip(Mword irqnum) const override
  {
    Mword origirq = irqnum;

    for (unsigned i = 0; i < _nr_blocks; ++i)
      {
        if (irqnum < _block[i].sz)
          return Irq(_block[i].chip, irqnum);

        irqnum -= _block[i].sz;
      }

    printf("KERNEL: exynos-irq: Invalid irqnum=%ld\n", origirq);
    return Irq();
  }

private:
  Gic *_gic;
  Combiner_chip *_cc;
  Gpio_wakeup_chip *_wu_gc;
  Gpio_eint_chip *_ei_gc1, *_ei_gc2;
};

PUBLIC
Mgr_int::Mgr_int()
{
  auto g = new Boot_object<Gic_v2>(Kmem_mmio::map(Mem_layout::Gic_cpu_int_phys_base,
                                                  Gic_cpu_v2::Size),
                                   Kmem_mmio::map(Mem_layout::Gic_dist_int_phys_base,
                                                  Gic_dist::Size));

  _gic = g;
  Pic::gic = g;

  _cc     = new Boot_object<Combiner_chip>();
  _wu_gc  = new Boot_object<Gpio_wakeup_chip>(Kmem::Gpio2_phys_base);
  _ei_gc1 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio1_phys_base,
                                                           0x1000), 16 * 8);
  _ei_gc2 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio2_phys_base,
                                                           0x1000), (29 - 21 + 1) * 8);

  // Combiners
  for (unsigned i = 0; i < 40; ++i)
    {
      _gic->alloc(new Boot_object<Combiner_cascade_irq>(i, _cc), i + 32);
      _gic->unmask(i + 32);
    }
  _gic->alloc(new Boot_object<Combiner_cascade_irq>(51, _cc), 51 + 32);
  _gic->unmask(51 + 32);
  _gic->alloc(new Boot_object<Combiner_cascade_irq>(53, _cc), 53 + 32);
  _gic->unmask(53 + 32);

  // GPIO-wakeup0-3 goes to GIC
  _gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 0), 72); _gic->unmask(72);
  _gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 1), 73); _gic->unmask(73);
  _gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 2), 74); _gic->unmask(74);
  _gic->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 3), 75); _gic->unmask(75);

  // GPIO-wakeup4-7 -> comb37:0-3
  for (unsigned i = 0; i < 4; ++i)
    {
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 4 + i), 8 * 37 + i);
      _cc->unmask(8 * 37 + i);
    }

  // GPIO-wakeup8-15 -> COMB:38:0-7
  for (unsigned i = 0; i < 8; ++i)
    {
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 8 + i), 8 * 38 + i);
      _cc->unmask(8 * 38 + i);
    }

  // GPIO-wakeup16-31: COMP:39:0
  _cc->alloc(new Boot_object<Gpio_cascade_wu23_irq>(_wu_gc), 8 * 39 + 0);
  _cc->unmask(8 * 39 + 0);

  // xa
  _cc->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc1), 8 * 24 + 1);
  _cc->unmask(8 * 24 + 1);

  // xb
  _cc->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc2), 8 * 24 + 0);
  _cc->unmask(8 * 24 + 0);

  static Chip_block soc[] = {
    { 96,                 _gic },
    { 54 * 8,             _cc },
    { 32,                 _wu_gc },
    { _ei_gc1->nr_irqs(), _ei_gc1 },
    { _ei_gc2->nr_irqs(), _ei_gc2 },
  };

  _block = soc;
  _nr_blocks = sizeof(soc) / sizeof(soc[0]);

  calc_nr_irq();
}


PUBLIC
void
Mgr_int::set_cpu(Mword irqnum, Cpu_number cpu) const override
{
  // this handles only the MCT_L[01] timers
  if (   irqnum == 379  // MCT_L1: Combiner 35:3
      || irqnum == 504) // MCT_L0: Combiner 51:0
    _gic->set_cpu(32 + (irqnum - 96) / 8, cpu);
  else
    WARNX(Warning, "IRQ%ld: ignoring CPU setting (%d).\n",
          irqnum, cxx::int_value<Cpu_number>(cpu));
}

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = new Boot_object<Mgr_int>();
}

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}

// ------------------------------------------------------------------------
INTERFACE [pf_exynos4 && exynos_extgic]:

#include "gic_v2.h"
#include "per_cpu_data.h"
#include "platform.h"

EXTENSION class Pic
{
public:
  static Per_cpu_ptr<Static_object<Gic_v2> > gic;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [pf_exynos4 && exynos_extgic]:

#include "cpu.h"

Per_cpu_ptr<Static_object<Gic_v2> > Pic::gic;

class Mgr_ext : public Mgr_exynos
{
public:
  Irq chip(Mword irqnum) const override
  {
    Mword origirq = irqnum;

    for (unsigned i = 0; i < _nr_blocks; ++i)
      {
        if (irqnum < _block[i].sz)
          {
            if (i == 0) // some special handling in GIC block
              if (!Platform::is_4412())
                if (irqnum == 80 && Config::Max_num_cpus > 1) // MCT_L1 goes to CPU1
                  return Irq(_gic.cpu(Cpu_number(1)), irqnum);

            return Irq(_block[i].chip, irqnum);
          }

        irqnum -= _block[i].sz;
      }

    printf("KERNEL: exynos-irq: Invalid irqnum=%ld\n", origirq);
    return Irq();
  }

  Unsigned32 wakeup_irq_eint_mask() { return _wu_gc->_wakeup; }

private:
  friend class Pic;
  static Per_cpu<Static_object<Gic_v2> > _gic;
  Combiner_chip *_cc;
  Gpio_wakeup_chip *_wu_gc;
  Gpio_eint_chip *_ei_gc1, *_ei_gc2;
  //Gpio_eint_chip *_ei_gc3, *_ei_gc4;
};

DEFINE_PER_CPU Per_cpu<Static_object<Gic_v2> > Mgr_ext::_gic;

PUBLIC
Mgr_ext::Mgr_ext()
{
  Gic *g = _gic.cpu(Cpu_number(0)).construct(
      Kmem_mmio::map(Mem_layout::Gic_cpu_ext_cpu0_phys_base, Gic_cpu_v2::Size),
      Kmem_mmio::map(Mem_layout::Gic_dist_ext_cpu0_phys_base, Gic_dist::Size));

  _cc = new Boot_object<Combiner_chip>();

  _wu_gc = new Boot_object<Gpio_wakeup_chip>(Kmem::Gpio2_phys_base);
  _ei_gc1 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio1_phys_base,
                                                           0x1000), 18 * 8);
  _ei_gc2 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio2_phys_base,
                                                           0x1000), 14 * 8);

  // Combiners
  for (unsigned i = 0; i < 16; ++i)
    {
      g->alloc(new Boot_object<Combiner_cascade_irq>(i, _cc), i + 32);
      g->unmask(i + 32);
    }

  if (Platform::is_4412())
    {
      g->alloc(new Boot_object<Combiner_cascade_irq>(16, _cc), 139); g->unmask(139);
      g->alloc(new Boot_object<Combiner_cascade_irq>(17, _cc), 140); g->unmask(140);
      g->alloc(new Boot_object<Combiner_cascade_irq>(18, _cc), 80);  g->unmask(80);
      g->alloc(new Boot_object<Combiner_cascade_irq>(19, _cc), 74);  g->unmask(74);
    }

  // GPIO-wakeup0-15 goes to GIC
  for (unsigned i = 0; i < 16; ++i)
    {
      g->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, i), i + 48);
      g->unmask(i + 48);
    }

  // GPIO-wakeup16-31: GIC:32+32
  g->alloc(new Boot_object<Gpio_cascade_wu23_irq>(_wu_gc), 64);
  g->unmask(64);

  // xa GIC:32+47
  g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc1, Platform::is_4412() ? 1 : 0), 79);
  g->unmask(79);

  // xb GIC:32+46
  g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc2, Platform::is_4412() ? 2 : 0), 78);
  g->unmask(78);


  // 4210
  //  - part1: ext-int 0x700: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
  //  - part2: ext-int 0x700: 21, 22, 23, 24, 25, 26, 27, 28, 29
  //                   0xe00: wu1, wu2, wu3, wu4
  //  - part3: nix

  static Chip_block soc4210[] = {
    { 160,               _gic.cpu(Cpu_number(0)) },
    { 16 * 8,            _cc },
    { 32,                _wu_gc },
    { 16 * 8,            _ei_gc1 },
    { (29 - 21 + 1) * 8, _ei_gc2 },
  };

  // 4412
  //  - part1: ext-int 0x700: 1, 2, 3, 4, 5, 6, 7, 13, 14, 15, 16, 21, 22
  //  - part2: ext-int 0x700: 23, 24, 25, 26, 27, 28, 29, 8, 9, 10, 11, 12
  //                   0xe00: 40, 41, 42, 43
  //  - part3: ext-int 0x700: 50
  //  - part4: ext-int 0x700: 30, 31, 32, 33, 34

  static Chip_block soc4412[] = {
    { 160,               _gic.cpu(Cpu_number(0)) },
    { 20 * 8,            _cc },
    {  4 * 8,            _wu_gc },
    { 18 * 8,            _ei_gc1 },
    { 14 * 8,            _ei_gc2 },
    //{  1 * 8,            _ei_gc3 }, // Do not know upstream IRQ-num :(
    //{  5 * 8,            _ei_gc4 }, // Do not know upstream IRQ-num :(
  };

  if (Platform::is_4412())
    {
      _block = soc4412;
      _nr_blocks = sizeof(soc4412) / sizeof(soc4412[0]);
    }
  else
    {
      _block = soc4210;
      _nr_blocks = sizeof(soc4210) / sizeof(soc4210[0]);
    }

  calc_nr_irq();
}

/**
 * \pre must run on the CPU given in \a cpu.
 */
PUBLIC
void
Mgr_ext::set_cpu(Mword irqnum, Cpu_number cpu) const override
{
  if (!Platform::is_4412() && irqnum == 80)  // MCT_L1
    _gic.cpu(cpu)->set_cpu(80, cpu);
  else
    WARNX(Warning, "IRQ%ld: ignoring CPU setting (%d).\n", irqnum,
          cxx::int_value<Cpu_number>(cpu));
}

PUBLIC static
void
Mgr_ext::exynos_irq_handler()
{ nonull_static_cast<Mgr_ext *>(Irq_mgr::mgr.unwrap())->_gic.current()->hit(0); }

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Mgr_ext *m = new Boot_object<Mgr_ext>();
  Irq_mgr::mgr = m;
  gic = &m->_gic;
  Gic::set_irq_handler(&Mgr_ext::exynos_irq_handler);
}

PUBLIC static
void
Pic::reinit(Cpu_number cpu)
{
  gic.cpu(cpu)->init(true, 96);
}

class Check_irq0 : public Irq_base
{
public:
  Check_irq0() { set_hit(&hndl); }
  static void hndl(Irq_base *, Upstream_irq const *)
  {
    printf("IRQ0 appeared on CPU%d\n",
           cxx::int_value<Cpu_number>(current_cpu()));
  }
private:
  void switch_mode(bool) override {}
};

DEFINE_PER_CPU static Per_cpu<Static_object<Check_irq0> > _check_irq0;


PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  if (!resume)
    {
      if (Platform::is_4412())
        {
          assert(cpu > Cpu_number(0));
          assert(cpu < Cpu_number(4));

          unsigned phys_cpu = cxx::int_value<Cpu_phys_id>(Cpu::cpus.cpu(cpu).phys_id());
          gic.cpu(cpu).construct(
              Kmem_mmio::map(Mem_layout::Gic_cpu_ext_cpu0_phys_base + phys_cpu * 0x4000,
                             0x1000),
              Kmem_mmio::map(Mem_layout::Gic_dist_ext_cpu0_phys_base + phys_cpu * 0x4000,
                             0x1000),
              gic.cpu(Cpu_number(0)));
        }
      else
        {
          assert (cpu == Cpu_number(1));
          assert (Cpu::cpus.cpu(cpu).phys_id() == Cpu_phys_id(1));

          gic.cpu(cpu).construct(Kmem_mmio::map(Mem_layout::Gic_cpu_ext_cpu1_phys_base,
                                                0x1000),
                                 Kmem_mmio::map(Mem_layout::Gic_dist_ext_cpu1_phys_base,
                                                0x1000),
                                 gic.cpu(Cpu_number(0)));
        }
    }

  gic.cpu(cpu)->init_ap(cpu, resume);


  if (!resume)
    {
      // This is a debug facility as we've been seeing IRQ0
      // happening under (non-usual) high load
      _check_irq0.cpu(cpu).construct();
      gic.cpu(cpu)->alloc(_check_irq0.cpu(cpu), 0);
    }
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && exynos_extgic && arm_em_tz]:

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  gic.current()->set_pending_irq(group32num, val);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [pf_exynos5]:

#include "gic_v2.h"
#include "platform.h"

class Mgr : public Mgr_exynos
{
  Irq chip(Mword irqnum) const override
  {
    Mword origirq = irqnum;

    for (unsigned i = 0; i < _nr_blocks; ++i)
      {
        if (irqnum < _block[i].sz)
          return Irq(_block[i].chip, irqnum);

        irqnum -= _block[i].sz;
      }

    printf("KERNEL: exynos-irq: Invalid irqnum=%ld\n", origirq);
    return Irq();
  }

private:
  friend class Pic;
  Combiner_chip *_cc;
  Gpio_wakeup_chip *_wu_gc;
  Gpio_eint_chip *_ei_gc1, *_ei_gc2;
  Gpio_eint_chip *_ei_gc3, *_ei_gc4;
};

PUBLIC
Mgr::Mgr()
{
  Gic *g = new Boot_object<Gic_v2>(Kmem_mmio::map(Mem_layout::Gic_cpu_phys_base,
                                                  Gic_cpu_v2::Size),
                                   Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                                  Gic_dist::Size));
  Pic::gic = g;

  _cc = new Boot_object<Combiner_chip>();

  _wu_gc = new Boot_object<Gpio_wakeup_chip>(Kmem::Gpio1_phys_base);

  _ei_gc1 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio1_phys_base,
                                                           0x1000), 13 * 8);
  _ei_gc2 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio2_phys_base,
                                                           0x1000),  8 * 8);
  _ei_gc3 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio3_phys_base,
                                                           0x1000),  5 * 8);
  _ei_gc4 = new Boot_object<Gpio_eint_chip>(Kmem_mmio::map(Mem_layout::Gpio4_phys_base,
                                                           0x1000),  1 * 8);

  // Combiners
  for (unsigned i = 0; i < 32; ++i)
    {
      g->alloc(new Boot_object<Combiner_cascade_irq>(i, _cc), i + 32);
      g->unmask(i + 32);
    }

  _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 0), 8 * 23 + 0);
  _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, 1), 8 * 24 + 0);
  for (int i = 25, nr = 2; i < 32; ++i, nr += 2)
    {
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, nr + 0), 8 * i + 0);
      _cc->alloc(new Boot_object<Gpio_cascade_wu01_irq>(_wu_gc, nr + 1), 8 * i + 1);
    }

  // GPIO-wakeup16-31: GIC:32+32
  g->alloc(new Boot_object<Gpio_cascade_wu23_irq>(_wu_gc), 64);
  g->unmask(64);

  if (0)
    {
      // xa GIC:32+47
      g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc1), 79);
      g->unmask(79);

      // xb GIC:32+46
      g->alloc(new Boot_object<Gpio_cascade_xab_irq>(_ei_gc2), 78);
      g->unmask(78);
    }

  // 5250
  //  - part1: ext-int 0x700: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 30
  //                   0xe00: 40, 41, 42, 43
  //  - part2: ext-int 0x700: 14, 15, 16, 17, 18, 19, 20, 21, 22
  //  - part3: ext-int 0x700: 60, 61, 62, 63, 64
  //  - part4: ext-int 0x700: 50

  static Chip_block socblock[] = {
    { Platform::is_5410() ? 256U : 160U, Pic::gic },
    { 32 * 8,                            _cc },
    { 32,                                _wu_gc },
    { _ei_gc1->nr_irqs(),                _ei_gc1 },
    { _ei_gc2->nr_irqs(),                _ei_gc2 },
    { _ei_gc3->nr_irqs(),                _ei_gc3 },
    { _ei_gc4->nr_irqs(),                _ei_gc4 },
  };

  _block = socblock;
  _nr_blocks = sizeof(socblock) / sizeof(socblock[0]);

  calc_nr_irq();
}

/**
 * \pre must run on the CPU given in \a cpu.
 */
PUBLIC
void
Mgr::set_cpu(Mword irqnum, Cpu_number cpu) const override
{
  // this handles only the MCT_L[0123] timers
  if (   irqnum == 152   // MCT_L0
      || irqnum == 153   // MCT_L1
      || irqnum == 154   // MCT_L2
      || irqnum == 155)  // MCT_L3
    Pic::gic->set_cpu(irqnum, cpu);
  else
    WARNX(Warning, "IRQ%ld: ignoring CPU setting (%d).\n",
	  irqnum, cxx::int_value<Cpu_number>(cpu));
}

PUBLIC static FIASCO_INIT
void Pic::init()
{
  Irq_mgr::mgr = new Boot_object<Mgr>();
}

PUBLIC static
void
Pic::reinit(Cpu_number)
{
  gic->init(true);
}

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [debug && pf_exynos]:

PUBLIC
char const *
Combiner_chip::chip_type() const override
{ return "Comb"; }

PUBLIC
char const *
Gpio_eint_chip::chip_type() const override
{ return "EI-Gpio"; }

PUBLIC
char const *
Gpio_wakeup_chip::chip_type() const override
{ return "WU-GPIO"; }
