/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 */
IMPLEMENTATION [mips && malta]:

#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mmio_register_block.h"

void __attribute__ ((noreturn))
platform_reset()
{
  enum
  {
    SOFTRES_REGISTER = 0x1f000500,
    GORESET          = 0x42,
  };

  Register_block<32> r(Kmem_mmio::remap(SOFTRES_REGISTER, sizeof(Unsigned32)));
  r[0] = GORESET;

  L4::infinite_loop();
}
