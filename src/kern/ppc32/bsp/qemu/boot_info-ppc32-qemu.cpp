IMPLEMENTATION[ppc32 && qemu]:

#include<ppc_types.h>

IMPLEMENT static
Address Boot_info::uart_base()
{
  return 0;
}
