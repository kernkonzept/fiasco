INTERFACE [pf_rpi_rpi2 || pf_rpi_rpi3 || pf_rpi_rpi4]: // -----------------

#include "arithmetic.h"
#include "assert.h"
#include "kmem_mmio.h"
#include "mem_layout.h"
#include "mmio_register_block.h"

class Arm_control
{
public:
  enum
  {
    nCNTPSIRQ       = 0,
    nCNTPNSIRQ      = 1,
    nCNTHPIRQ       = 2,
    nCNTVIRQ        = 3,
    Local_timer_irq = nCNTVIRQ,

    Control                  = 0x00,
    Prescaler                = 0x08,
    Timer_irq_base           = 0x40,
    Mailbox_irq_control_base = 0x50,
    Irq_pending_base         = 0x60,
    Fiq_pending_base         = 0x70,
    Mailbox_set_base         = 0x80,
    Mailbox_rd_clr_base      = 0xc0,
  };

  Arm_control()
  : r(Kmem_mmio::map(Mem_layout::Local_intc, 0x100))
  {
    r.r<32>(Control) = 0;
    r.r<32>(Prescaler) =  1 << 31;
  }

  static void init()
  { _arm_control.construct(); };

  static Arm_control *o() { return _arm_control.get(); }

private:
  Mmio_register_block r;

  static Static_object<Arm_control> _arm_control;
};

INTERFACE [pf_rpi_rpi2 || pf_rpi_rpi3]: // --------------------------------

EXTENSION class Arm_control
{
private:
  static unsigned cpu_off(Cpu_phys_id cpu)
  { return (cxx::int_value<Cpu_phys_id>(cpu) & 0xffu) * 4u; }

public:
  void timer_mask(unsigned timer, Cpu_phys_id cpu = Proc::cpu_id())
  {
    r.r<32>(Timer_irq_base + cpu_off(cpu)).clear(1 << timer);
  }

  void timer_unmask(unsigned timer, Cpu_phys_id cpu = Proc::cpu_id())
  {
    r.r<32>(Timer_irq_base + cpu_off(cpu)).set(1 << timer);
  }

  void mailbox_mask(unsigned mbox, Cpu_phys_id cpu = Proc::cpu_id())
  {
    assert(mbox < 4);
    r.r<32>(Mailbox_irq_control_base + cpu_off(cpu)).clear(1 << mbox);
  }

  void mailbox_unmask(unsigned mbox, Cpu_phys_id cpu = Proc::cpu_id())
  {
    assert(mbox < 4);
    r.r<32>(Mailbox_irq_control_base + cpu_off(cpu)).set(1 << mbox);
  }

  Unsigned32 irqs_pending(Cpu_phys_id cpu = Proc::cpu_id())
  { return r.r<32>(Irq_pending_base + cpu_off(cpu)); }

  void send_ipi(unsigned ipi_msg, Cpu_phys_id to_cpu)
  {
    Mem::dsb();
    unsigned cpu_num = cxx::int_value<Cpu_phys_id>(to_cpu) & 0xffu;
    r.r<32>(Mailbox_set_base + 16 * cpu_num) = 1 << ipi_msg;
  }

  unsigned ipi_pending()
  {
    unsigned cpu_num = cxx::int_value<Cpu_phys_id>(Proc::cpu_id()) & 0xffu;
    unsigned mbox0_rdclk = Mailbox_rd_clr_base + 16 * cpu_num;

    Unsigned32 v = r.r<32>(mbox0_rdclk);
    unsigned m = cxx::log2u(v);
    r.r<32>(mbox0_rdclk) = 1 << m;
    return m;
  }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [pf_rpi_rpi2 || pf_rpi_rpi3 || (pf_rpi_rpi4 && !64bit)]:

Static_object<Arm_control> Arm_control::_arm_control;

// ------------------------------------------------------------------------
IMPLEMENTATION [mp && (pf_rpi_rpi2 || pf_rpi_rpi3 || pf_rpi_rpi4) && !64bit]:

PUBLIC
void
Arm_control::do_boot_cpu(Cpu_phys_id phys_cpu, Address paddr)
{
  unsigned cpu_num = cxx::int_value<Cpu_phys_id>(phys_cpu);
  r.r<32>(Mailbox_set_base + 0xc + cpu_num * 0x10) = paddr;
  Mem::dsb();
  asm volatile("sev");
}
