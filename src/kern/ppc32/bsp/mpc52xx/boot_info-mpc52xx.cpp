IMPLEMENTATION[ppc32 && mpc52xx]:

#include<ppc_types.h>

IMPLEMENT static
Address Boot_info::uart_base()
{
  Of_device *dev = get_device((char*)"serial", (char*)"serial");
  return dev ? dev->reg : 0;
}

IMPLEMENT static
Address Boot_info::pic_base()
{
  Of_device *dev = get_device((char*)"pic", NULL);
  return dev ? dev->reg : 0;
}

PUBLIC static
Address Boot_info::mbar()
{
  Of_device *dev = get_device("builtin", NULL);

  return dev ? dev->reg : 0;
}
