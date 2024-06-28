IMPLEMENTATION [arm && pf_s32g && !arm_psci]:

#include "infinite_loop.h"
#include "mmio_register_block.h"
#include "kmem_mmio.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Mmio_register_block rgm(Kmem_mmio::map(0x40078000, 0x20));
  rgm.r<8>(0x18) = 0xf;

  Mmio_register_block me(Kmem_mmio::map(0x40088000, 16));
  me.r<32>(4) = 2;
  me.r<32>(8) = 1;
  me.r<32>(0) = 0x5AF0;
  me.r<32>(0) = 0xA50F;

  L4::infinite_loop();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32g && arm_psci]:

#include "infinite_loop.h"
#include "psci.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  Psci::system_reset();
  L4::infinite_loop();
}
