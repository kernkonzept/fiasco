IMPLEMENTATION [arm && pf_omap3_35x]: //-----------------------------------

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum
  {
    PRM_RSTCTRL = Mem_layout::Prm_global_reg_phys_base + 0x50,
  };

  Io::write<Mword>(2, Kmem::mmio_remap(PRM_RSTCTRL));

  for (;;)
    ;
}

IMPLEMENTATION [arm && pf_omap3_am33xx]: //--------------------------------

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum { PRM_RSTCTRL = 0x44e00F00, };

  Io::write<Mword>(1, Kmem::mmio_remap(PRM_RSTCTRL));

  for (;;)
    ;
}

IMPLEMENTATION [arm && pf_omap4]: //---------------------------------------

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum
  {
    DEVICE_PRM  = Mem_layout::Prm_phys_base + 0x1b00,
    PRM_RSTCTRL = DEVICE_PRM + 0,
  };
  Address p = Kmem::mmio_remap(PRM_RSTCTRL);

  Io::set<Mword>(1, p);
  Io::read<Mword>(p);

  for (;;)
    ;
}

IMPLEMENTATION [arm && pf_omap5]: //---------------------------------------

#include "io.h"
#include "kmem.h"

void __attribute__ ((noreturn))
platform_reset(void)
{
  enum
  {
    DEVICE_PRM         = Mem_layout::Prm_phys_base + 0x1c00,
    PRM_RSTCTRL        = DEVICE_PRM + 0,
    RST_GLOBAL_COLD_SW = 1 << 1,
  };
  Address p = Kmem::mmio_remap(PRM_RSTCTRL);

  Io::set<Mword>(RST_GLOBAL_COLD_SW, p);
  Io::read<Mword>(p);

  for (;;)
    ;
}
