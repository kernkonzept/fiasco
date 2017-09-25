INTERFACE:

EXTENSION class Kernel_uart { enum { Bsp_init_mode = Init_before_mmu }; };

IMPLEMENTATION [ppc32 && serial]:

IMPLEMENT
bool Kernel_uart::startup(unsigned, int /*irq*/)
{
  return Uart::startup();
}
