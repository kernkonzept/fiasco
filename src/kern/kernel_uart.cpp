INTERFACE:

class Kernel_uart
{
public:
  enum Init_mode
  {
    Init_before_mmu,
    Init_after_mmu
  };
  Kernel_uart();
  static void enable_rcv_irq();
};

INTERFACE [serial]:

#include "uart.h"
#include "std_macros.h"
#include "pm.h"

class Filter_console;

/**
 * Glue between kernel and UART driver.
 */
EXTENSION class Kernel_uart : public Uart, public Pm_object
{
private:
  /**
   * Prototype for the UART specific startup implementation.
   *
   * \param port    the COM port number.
   * \param irq     the UART IRQ number.
   * \param resume  true, if called during a wakeup from suspend.
   */
  bool startup(unsigned port, int irq, bool resume);

  static bool init_for_mode(Init_mode init_mode);
};

//---------------------------------------------------------------------------
IMPLEMENTATION [serial]:

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "filter_console.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "kdb_ke.h"
#include "kernel_console.h"
#include "uart.h"
#include "config.h"
#include "kip.h"
#include "koptions.h"
#include "panic.h"
#include "vkey.h"

static DEFINE_GLOBAL Global_data<Static_object<Filter_console>> _fcon;
static DEFINE_GLOBAL Global_data<Static_object<Kernel_uart>> _kernel_uart;

PUBLIC static FIASCO_CONST
Uart *
Kernel_uart::uart()
{ return _kernel_uart; }

PUBLIC static FIASCO_CONST
Filter_console *
Kernel_uart::fcon()
{ return _fcon; }

IMPLEMENT_DEFAULT inline
bool
Kernel_uart::init_for_mode(Init_mode init_mode)
{ return (int)init_mode == Bsp_init_mode; }

PUBLIC static
bool
Kernel_uart::init(Init_mode init_mode = Init_before_mmu)
{
  if (!init_for_mode(init_mode))
    return false;

  if (Koptions::o()->opt(Koptions::F_noserial)) // do not use serial uart
    return true;

  _kernel_uart.construct();
  _fcon.construct(_kernel_uart);

  Kconsole::console()->register_console(_fcon, 0);
  return true;
}

PUBLIC
void
Kernel_uart::setup(bool resume)
{
  unsigned           n = Config::default_console_uart_baudrate;
  Uart::TransferMode m = Uart::MODE_8N1;
  unsigned long long p = Config::default_console_uart;
  int                i = -1;

  if (Koptions::o()->opt(Koptions::F_uart_baud))
    n = Koptions::o()->uart.baud;

  if (Koptions::o()->opt(Koptions::F_uart_base))
    p = Koptions::o()->uart.base_address;

  if (Koptions::o()->opt(Koptions::F_uart_irq))
    i = Koptions::o()->uart.irqno;

  if (!startup(p, i, resume))
    printf("Comport/base 0x%04llx is not accepted by the uart driver!\n", p);
  else if (!change_mode(m, n))
    panic("Something is wrong with the baud rate (%u)!\n", n);
}

IMPLEMENT
Kernel_uart::Kernel_uart()
{
  setup(false);
  register_pm(Cpu_number::boot_cpu());
}

PUBLIC void
Kernel_uart::pm_on_suspend([[maybe_unused]] Cpu_number cpu) override
{
  assert (cpu == Cpu_number::boot_cpu());

  uart()->state(Console::DISABLED);

  if(Config::serial_esc != Config::SERIAL_ESC_NOIRQ)
    uart()->disable_rcv_irq();
}

PUBLIC void
Kernel_uart::pm_on_resume([[maybe_unused]] Cpu_number cpu) override
{
  assert (cpu == Cpu_number::boot_cpu());
  static_cast<Kernel_uart*>(Kernel_uart::uart())->setup(true);
  uart()->state(Console::ENABLED);

  if(Config::serial_esc != Config::SERIAL_ESC_NOIRQ)
    uart()->enable_rcv_irq();
}


class Kuart_irq : public Irq_base
{
public:
  Kuart_irq() { hit_func = &handler_wrapper<Kuart_irq>; }
  void switch_mode(bool) override {}
  void handle(Upstream_irq const *ui)
  {
    Kernel_uart::uart()->irq_ack();
    mask_and_ack();
    Upstream_irq::ack(ui);
    unmask();
    if (!Vkey::check_())
      kdb_ke("IRQ ENTRY");
  }
};

static DEFINE_GLOBAL_PRIO(BOOTSTRAP_INIT_PRIO) Global_data<Kuart_irq> uart_irq;

IMPLEMENT
void
Kernel_uart::enable_rcv_irq()
{
  Irq_mgr *mgr = Irq_mgr::mgr;
  if (mgr->alloc(&uart_irq, mgr->legacy_override(uart()->irq())))
    {
      uart_irq->unmask();
      uart()->enable_rcv_irq();
      Vkey::enable_receive();
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!serial]:

PUBLIC static
bool
Kernel_uart::init(Init_mode = Init_before_mmu)
{ return false; }

IMPLEMENT inline
Kernel_uart::Kernel_uart()
{}

IMPLEMENT inline
void
Kernel_uart::enable_rcv_irq()
{}
