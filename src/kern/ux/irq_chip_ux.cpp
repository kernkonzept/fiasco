INTERFACE:

#include "config.h"
#include <sys/poll.h>

#include "irq_chip_ux_base.h"
#include "irq_chip.h"
#include "irq_chip_ia32.h"
#include "irq_mgr.h"

class Irq_chip_ux :
  public Irq_chip_ux_base,
  public Irq_chip_icu,
  private Irq_chip_ia32,
  private Irq_mgr
{
  friend class Irq_chip_ia32;
public:
  bool reserve(Mword pin) override { return Irq_chip_ia32::reserve(pin); }
  Irq_base *irq(Mword pin) const override { return Irq_chip_ia32::irq(pin); }
  unsigned nr_irqs() const override { return Num_irqs; }
  unsigned nr_msis() const override { return 0; }

  void mask(Mword) override {}
  void ack(Mword) override {}
  void mask_and_ack(Mword) override {}
  int set_mode(Mword, Mode) override { return 0; }
  bool is_edge_triggered(Mword) const override { return false; }
  void set_cpu(Mword, Cpu_number) override {}

  Irq_mgr::Irq chip(Mword pin) const override
  {
    if (pin < Num_irqs)
      return Irq(const_cast<Irq_chip_ux*>(this), pin);

    return Irq();
  }
};


IMPLEMENTATION:

#include <cassert>
#include <cstdlib>
#include <csignal>
#include <fcntl.h>

#include "boot_info.h"

PUBLIC
bool
Irq_chip_ux::alloc(Irq_base *irq, Mword irqn)
{
  if (irqn >= Num_irqs)
    return false;

  // PIC uses 16 vectors from Base_vector statically
  unsigned vector = _base_vect + irqn;
  return valloc<Irq_chip_ux>(irq, irqn, vector);
}


PUBLIC
void
Irq_chip_ux::unmask(Mword pin)
{
  assert (pin < Num_irqs);
  auto &p = pfd[pin];
  // If fd is zero, someone tried to activate an IRQ without provider
  assert (p.fd);

  int flags = fcntl(p.fd, F_GETFL);
  if (   flags < 0
      || fcntl(p.fd, F_SETFL, flags | O_NONBLOCK | O_ASYNC) < 0
      || fcntl(p.fd, F_SETSIG, SIGIO) < 0
      || fcntl(p.fd, F_SETOWN, Boot_info::pid()) < 0)
    return;

  p.events = POLLIN;
  if (pin >= highest_irq)
    highest_irq = pin + 1;
}
static void irq_prov_shutdown()
{
  if (Irq_chip_ux::main)
    Irq_chip_ux::main->shutdown();
}

PUBLIC
Irq_chip_ux::Irq_chip_ux(bool is_main) : Irq_chip_ia32(Num_irqs)
{
  if (is_main)
    {
      main = this;
      Irq_mgr::mgr = this;
      atexit(irq_prov_shutdown);
    }
}

// -----------------------------------------------------------------
IMPLEMENTATION [ux && debug]:

PUBLIC
char const *
Irq_chip_ux::chip_type() const
{ return "UX"; }

