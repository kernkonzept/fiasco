IMPLEMENTATION [arm && pf_integrator]:

#include "infinite_loop.h"
#include "io.h"
#include "kmem_mmio.h"

[[noreturn]] void
platform_reset(void)
{
  enum {
    HDR_CTRL_OFFSET = Mem_layout::Integrator_phys_base + 0xc,
  };

  Io::write(1 << 3, Kmem_mmio::map(HDR_CTRL_OFFSET, sizeof(Mword)));

  L4::infinite_loop();
}
