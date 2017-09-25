/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */
IMPLEMENTATION [mips && malta]:

#include "kmem.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset()
{
  enum
  {
    SOFTRES_REGISTER = 0x1f000500,
    GORESET          = 0x42,
  };

  Register_block<32> r(Kmem::mmio_remap(SOFTRES_REGISTER));
  r[0] = GORESET;

  for(;;)
    ;
}
