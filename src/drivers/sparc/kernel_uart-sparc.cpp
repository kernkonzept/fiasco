INTERFACE:

EXTENSION class Kernel_uart { enum { Bsp_init_mode = Init_before_mmu }; };

IMPLEMENTATION [sparc && serial]:

IMPLEMENT
bool Kernel_uart::startup(unsigned, int)
{
  return Uart::startup();
}
