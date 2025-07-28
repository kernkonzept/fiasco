IMPLEMENTATION:

#include "dt.h"
#include "io_regblock.h"
#include "kmem_mmio.h"
#include "uart_tegra-tcu.h"
#include "warn.h"

IMPLEMENT_OVERRIDE
void
Uart::bsp_init_2(L4::Uart *uart)
{
  auto *the_uart = static_cast<L4::Uart_tegra_tcu *>(uart);
  // shared mailboxes start at base + 64 KiB
  void *m = Kmem_mmio::map(Mem_layout::Hsp_base + 0x10000, 0x1000);
  static L4::Io_register_block_mmio rx_mbox(m);

  the_uart->set_rx_mbox(&rx_mbox);
}
