INTERFACE [ppc32 && mpc52xx]:

#include "types.h"
#include "initcalls.h"
#include "irq_chip.h"
#include "irq_chip_generic.h"

EXTENSION class Pic : public Irq_chip_gen
{
public:
  static Pic *main;

private:
  /** Pic interrupt control registers (incomplete) */
  ///+0x00: Peripheral Interrupt Mask Register
  Address per_mask() { return _pic_base; }
  ///+0x04: Peripheral Priority & HI/LO Select Register1
  Address per_prio1() { return _pic_base + 0x04; }
  ///+0x08: Peripheral Priority & HI/LO Select Register2
  Address per_prio2() { return _pic_base + 0x08; }
  ///+0x0c: Peripheral Priority & HI/LO Select Register3
  Address per_prio3() { return _pic_base + 0x0c; }
  ///+0x10: External Enable & External Types Register
  Address ext() { return _pic_base + 0x10; }
  ///+0x14: Critical Priority & Main Inter. Mask Register
  Address main_mask() { return _pic_base + 0x14; }
  ///+0x18: Main Inter. Priority1
  Address main_prio1() { return _pic_base + 0x18; }
  ///+0x1c: Main Inter. Priority2
  Address main_prio2() { return _pic_base + 0x1c; }
  ///+0x24: PerStat, MainStat, CritStat Encoded Registers
  Address stat() { return _pic_base + 0x24; }

  /** Interrupt lines (sdma is missing) */
  enum Pic_lines
  {
    IRQ_CRIT = 0x0, ///Critical line
    IRQ_MAIN = 0x1, ///Main line
    IRQ_PER  = 0x2, ///Periphal line
    IRQ_SHIFT = 0x4
  };

  enum Pic_num_per_line
  {
    NUM_CRIT = 4,
    NUM_MAIN = 17,
    NUM_PER  = 24,
  };

  /** Interrupt senses */
  enum Pic_sense
  {
    SENSE_LEVEL_HIGH   = 0,
    SENSE_EDGE_RISING  = 1,
    SENSE_EDGE_FALLING = 2,
    SENSE_LEVEL_LOW    = 3
  };

  Address _pic_base;

  enum { IRQ_MAX  = (IRQ_PER << IRQ_SHIFT) + NUM_PER};

  static unsigned pic_line(unsigned irq_num)
  { return irq_num >> IRQ_SHIFT; }

  static unsigned irq_num(unsigned line, unsigned pic_num)
  { return (line << IRQ_SHIFT) | pic_num; }

  static inline unsigned pic_num(unsigned irq_num)
  { return irq_num & ~(~0U << IRQ_SHIFT); }

public:
  int set_mode(Mword, Mode) { return 0; }
  bool is_edge_triggered(Mword) const { return false; }
  void set_cpu(Mword, Cpu_number) {}
};

//------------------------------------------------------------------------------
IMPLEMENTATION [ppc32 && mpc52xx]:

#include "boot_info.h"
#include "io.h"
#include "irq.h"
#include "irq_chip_generic.h"
#include "irq_mgr.h"
#include "mmu.h"
#include "panic.h"
#include "ppc_types.h"

#include <cassert>
#include <cstdio>

Pic *Pic::main;


PRIVATE inline NOEXPORT
void
Pic::dispatch_mask(unsigned irq_num, Address *addr, unsigned *bit_offs)
{
  switch(pic_line(irq_num))
    {
    case IRQ_MAIN:
      *addr = main_mask();
      *bit_offs = 16;
      break;
    case IRQ_PER:
      *addr = per_mask();
      *bit_offs = 31;
      break;
    default:
      panic("%s: Cannot handle IRQ %u", __PRETTY_FUNCTION__, irq_num);
    }
}

PRIVATE inline NOEXPORT
void
Pic::set_stat_msb(unsigned irq_num)
{
  switch(pic_line(irq_num))
    {
    case IRQ_MAIN:
      Io::set<Unsigned32>(0x00200000, stat());
      break;
    case IRQ_PER:
      Io::set<Unsigned32>(0x20000000, stat());
      break;
    default:
      panic("CRIT not implemented");
    }
}

PUBLIC
void
Pic::mask(Mword irq_num)
{
  Address addr;
  unsigned bit_offs;
  dispatch_mask(irq_num, &addr, &bit_offs);
  Io::set<Unsigned32>(1U << (bit_offs - pic_num(irq_num)), addr);
}

PUBLIC
void
Pic::ack(Mword irq_num)
{
  unsigned line = pic_line(irq_num);
  unsigned num  = pic_num(irq_num);

  if((line == IRQ_MAIN && (num >= 1 || num <= 3)) ||
     (line == IRQ_CRIT && num == 0))
    Io::set<Unsigned32>(1U << (27 - num), ext());

  set_stat_msb(irq_num);
}

PUBLIC
void
Pic::mask_and_ack(Mword irq_num)
{
  Pic::mask(irq_num);
  Pic::ack(irq_num);
}

PUBLIC
void
Pic::unmask(Mword irq_num)
{
  Address addr;
  unsigned bit_offs;
  dispatch_mask(irq_num, &addr, &bit_offs);
  Io::clear<Unsigned32>(1U << (bit_offs - pic_num(irq_num)), addr);
}

PUBLIC explicit
Pic::Pic(Address base)
: Irq_chip_gen(IRQ_MAX), _pic_base(base)
{
  Mmu_guard dcache;
  Io::write_dirty<Unsigned32>(0xffffe000, per_mask());  //disable 0-19
  Io::write_dirty<Unsigned32>(0x00010fff, main_mask()); //disable 15, 20-31

  Unsigned32 ext_val = Io::read_dirty<Unsigned32>(ext());
  ext_val &= 0x00ff0000; //keep sense configuration
  ext_val |= 0x0f000000; //clear IRQ 0-3
  ext_val |= 0x00001000; //enable Master External Enable
  ext_val |= 0x00000f00; //enable all interrupt lines (IRQ 0-3)
  ext_val |= 0x00000001; //CEb route critical interrupt normally
  Io::write_dirty<Unsigned32>(ext_val, ext());

  //zero priority registers
  Io::write_dirty<Unsigned32>(0, per_prio1());
  Io::write_dirty<Unsigned32>(0, per_prio2());
  Io::write_dirty<Unsigned32>(0, per_prio3());
  Io::write_dirty<Unsigned32>(0, main_prio1());
  Io::write_dirty<Unsigned32>(0, main_prio2());
}

static Static_object<Irq_mgr_single_chip<Pic> > mgr;

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  assert(Boot_info::pic_base());
  Irq_mgr::mgr = mgr.construct(Boot_info::pic_base());
}

//------------------------------------------------------------------------------
/**
 * Irq number translations
 */

PUBLIC static
int
Pic::get_irq_num(const char *name, const char *type)
{
  Of_device *dev = Boot_info::get_device(name, type);

  if(!dev)
    return -1;

  return (int)Pic::irq_num(dev->interrupt[0], dev->interrupt[1]);
}

PRIVATE inline NOEXPORT
Unsigned32
Pic::pending_per(Unsigned32 state)
{
  Unsigned32 irq = state >> 24 & 0x1f; //5 bit

  if(irq  == 0)
    panic("No support for bestcomm interrupt, yet\n");

  return irq_num(IRQ_PER, irq);
}

PRIVATE inline NOEXPORT
Unsigned32
Pic::pending_main(Unsigned32 state)
{
  Unsigned32 irq = state >> 16 & 0x1f;

  //low periphal
  if(irq == 4)
    return pending_per(state);

  return irq_num(IRQ_MAIN, irq);
}

PUBLIC
void
Pic::post_pending_irqs()
{
  Unsigned32 irq;
  Unsigned32 state = Io::read<Unsigned32>(stat());

  //critical interupt
  if(state & 0x00000400)
    panic("No support for critical interrupt, yet");
  //main interrupt
  else if(state & 0x00200000)
    irq = pending_main(state);
  //periphal interrupt
  else if(state & 0x20000000)
    irq = pending_per(state);
  else
    return;

  Irq_base *i = nonull_static_cast<Irq_base*>(this->irq(irq));
  i->hit(0);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug && ppc32]:

PUBLIC
char const *
Pic::chip_type() const
{ return "Mpc52xx"; }
