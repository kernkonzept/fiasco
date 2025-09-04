IMPLEMENTATION [arm && pf_fvp_base]:

#include "global_data.h"
#include "infinite_loop.h"
#include "kmem_mmio.h"
#include "mem_layout.h"
#include "ve_sysregs.h"

static DEFINE_GLOBAL_PRIO(STARTUP_INIT_PRIO)
Global_data<Ve_sysregs> ve_sysregs(Kmem_mmio::map(Mem_layout::Ve_sysregs_base,
                                                  Mem_layout::Ve_sysregs_size));

void
platform_shutdown(void)
{
  ve_sysregs->shutdown();
}

[[noreturn]] void
platform_reset()
{
  ve_sysregs->reboot();
  L4::infinite_loop();
}
