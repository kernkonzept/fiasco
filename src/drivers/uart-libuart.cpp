INTERFACE [libuart]:

#include "io_regblock.h"
#include "global_data.h"
#include "types.h"

EXTENSION class Uart
{
private:
  static void init_uart_instance(Unsigned32 base_baud);
  /** Called _before_ executing startup() of the UART driver. */
  void bsp_init();

  /** Called _after_ executing startup() of the UART driver. */
  void bsp_init_2(L4::Uart *);

  int _irq = -1;
  static Global_data<L4::Uart *> _uart;
};

//------------------------------------------------------
IMPLEMENTATION [libuart]:

DEFINE_GLOBAL Global_data<L4::Uart *> Uart::_uart;

IMPLEMENT
Uart::Uart() : Console(DISABLED) {}

IMPLEMENT inline
int Uart::irq() const
{
  return _irq;
}

IMPLEMENT_DEFAULT
void
Uart::bsp_init()
{}

IMPLEMENT_DEFAULT
void
Uart::bsp_init_2(L4::Uart *)
{}

PROTECTED bool
Uart::startup(L4::Io_register_block const *reg, int irq, Unsigned32 base_baud,
              bool resume)
{
  _irq = irq;

  bsp_init();

  if (!resume)
    init_uart_instance(base_baud);

  if (!_uart->startup(reg))
    return false;

  bsp_init_2(_uart);

  add_state(ENABLED);
  return true;
}

IMPLEMENT L4::Uart *
Uart::uart()
{
  return _uart;
}

//------------------------------------------------------
IMPLEMENTATION [libuart && !dyn_uart]:

DEFINE_GLOBAL Global_data<Static_object<FIASCO_UART_TYPE>> _the_uart;

IMPLEMENT void inline
Uart::init_uart_instance(Unsigned32 base_baud)
{
  _uart = _the_uart.construct(base_baud);
}

//------------------------------------------------------
IMPLEMENTATION [libuart && dyn_uart]:

#include "device.h"
#include "koptions.h"

IMPLEMENT void inline
Uart::init_uart_instance(Unsigned32 base_baud)
{
  _uart = l4re_dev_uart_create_by_dt_compatible_once(Koptions::o()->uart.compatible_id,
                                                     base_baud);
}
