INTERFACE [libuart]:

#include "io_regblock_asi.h"

EXTENSION class Uart
{
  L4::Io_register_block_asi _regs;
};

IMPLEMENTATION [libuart]:

IMPLEMENT inline Uart::Uart() : Console(DISABLED), _regs(base()) {}

PUBLIC bool Uart::startup()
{
  if (uart()->startup(&_regs))
    {
      add_state(ENABLED);
      return true;
    }
  return false;
}
