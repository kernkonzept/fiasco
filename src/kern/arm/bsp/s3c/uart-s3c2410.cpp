IMPLEMENTATION [pf_s3c2410]:

#include "uart_s3c2410.h"
#include "mem_layout.h"

IMPLEMENT Address Uart::base() const { return Mem_layout::Uart_phys_base; }

IMPLEMENT int Uart::irq() const { return 28; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_s3c2410 uart;
  return &uart;
}
