IMPLEMENTATION [arm && (omap3_35xevm || omap3_am33xx)]: // ----------------

IMPLEMENT int Uart::irq() const { return 72; }

IMPLEMENTATION [arm && omap3_beagleboard]: // -----------------------------

IMPLEMENT int Uart::irq() const { return 74; }

IMPLEMENTATION [arm && omap4_pandaboard]: // ------------------------------

IMPLEMENT int Uart::irq() const { return 32 + 74; }

IMPLEMENTATION [arm && omap5]: // -----------------------------------------

IMPLEMENT int Uart::irq() const { return 32 + 74; }

IMPLEMENTATION: // --------------------------------------------------------

#include "mem_layout.h"
#include "uart_omap35x.h"

IMPLEMENT Address Uart::base() const { return Mem_layout::Uart_phys_base; }

IMPLEMENT L4::Uart *Uart::uart()
{
  static L4::Uart_omap35x uart;
  return &uart;
}
