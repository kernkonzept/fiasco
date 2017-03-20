INTERFACE [arm && pf_rcar3]:

// On ARM the MMIO for the uart is accessible before the MMU is fully up
EXTENSION class Kernel_uart { enum { Bsp_init_mode = Init_before_mmu }; };

IMPLEMENTATION [arm && serial && pf_rcar3]:

IMPLEMENT
bool Kernel_uart::startup(unsigned, int)
{
  return Uart::startup();
}
