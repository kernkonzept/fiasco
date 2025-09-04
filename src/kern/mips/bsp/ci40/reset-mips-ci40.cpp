IMPLEMENTATION [mips && ci40]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

[[noreturn]] void
platform_reset()
{
  Register_block<32> wdg(Kmem_mmio::map(0x18102100, sizeof(Unsigned32)));
  wdg[0] = 1;

  L4::infinite_loop();
}
