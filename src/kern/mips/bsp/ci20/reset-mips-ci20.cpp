IMPLEMENTATION [mips && ci20]:

#include "infinite_loop.h"
#include "kmem.h"
#include "mmio_register_block.h"
#include <cstdio>

[[noreturn]] void
platform_reset()
{
  printf("TODO: Implement reset\n");

  L4::infinite_loop();
}
