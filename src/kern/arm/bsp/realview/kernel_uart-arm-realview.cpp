INTERFACE:

// On ARM the MMIO for the uart is accessible before the MMU is fully up
EXTENSION class Kernel_uart { enum { Bsp_init_mode = Init_before_mmu }; };

IMPLEMENTATION [arm && realview && serial]:

IMPLEMENT
bool Kernel_uart::startup(unsigned, int)
{
  return Uart::startup();
}
