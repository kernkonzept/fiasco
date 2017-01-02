INTERFACE:

// On ARM the MMIO for the uart is accessible before the MMU is fully up
EXTENSION class Kernel_uart { enum { Bsp_init_mode = Init_before_mmu }; };

IMPLEMENTATION [arm && pf_sa1100 && serial]:

#include "kmem.h"
#include "mem_layout.h"

IMPLEMENT
bool Kernel_uart::startup(unsigned /*port*/, int /*irq*/)
{
  return Uart::startup(Kmem::mmio_remap(Mem_layout::Uart_phys_base), 17);
}
