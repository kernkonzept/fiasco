IMPLEMENTATION [zynq]:

#include "uart_cadence.h"
#include "mem_layout.h"

IMPLEMENT Address Uart::base() const { return Mem_layout::Uart_phys_base; }

IMPLEMENT int Uart::irq() const
{
  switch (CONFIG_PF_ZYNQ_UART_NR)
    {
    case 0: return 59;
    default:
    case 1: return 82;
    };
}

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_cadence uart;
  return &uart;
}
