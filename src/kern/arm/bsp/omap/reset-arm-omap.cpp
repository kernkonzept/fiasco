IMPLEMENTATION [arm && pf_omap3_35x]: //-----------------------------------

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset()
{
  enum
  {
    PRM_RSTCTRL = Mem_layout::Prm_global_reg_phys_base + 0x50,
  };

  Io::write<Mword>(2, Kmem_mmio::map(PRM_RSTCTRL, sizeof(Mword)));

  L4::infinite_loop();
}

IMPLEMENTATION [arm && pf_omap3_am33xx]: //--------------------------------

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset()
{
  enum { PRM_RSTCTRL = 0x44e00F00, };

  Io::write<Mword>(1, Kmem_mmio::map(PRM_RSTCTRL, sizeof(Mword)));

  L4::infinite_loop();
}

IMPLEMENTATION [arm && pf_omap4]: //---------------------------------------

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset()
{
  enum
  {
    DEVICE_PRM  = Mem_layout::Prm_phys_base + 0x1b00,
    PRM_RSTCTRL = DEVICE_PRM + 0,
  };

  void *p = Kmem_mmio::map(PRM_RSTCTRL, sizeof(Mword));

  Io::set<Mword>(1, p);
  Io::read<Mword>(p);

  L4::infinite_loop();
}

IMPLEMENTATION [arm && pf_omap5]: //---------------------------------------

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset()
{
  enum
  {
    DEVICE_PRM         = Mem_layout::Prm_phys_base + 0x1c00,
    PRM_RSTCTRL        = DEVICE_PRM + 0,
    RST_GLOBAL_COLD_SW = 1 << 1,
  };

  void *p = Kmem_mmio::map(PRM_RSTCTRL, sizeof(Mword));

  Io::set<Mword>(RST_GLOBAL_COLD_SW, p);
  Io::read<Mword>(p);

  L4::infinite_loop();
}
