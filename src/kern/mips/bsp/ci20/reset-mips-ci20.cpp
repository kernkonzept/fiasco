IMPLEMENTATION [mips && ci20]:

#include "kmem.h"
#include "mmio_register_block.h"
#include <cstdio>

void __attribute__ ((noreturn))
platform_reset()
{
  printf("TODO: Implement reset\n");

  for(;;)
    ;
}
