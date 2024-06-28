INTERFACE [riscv]:

#include "initcalls.h"

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "boot_alloc.h"
#include "irq_mgr.h"
#include "irq_migv.h"
#include "kmem_mmio.h"

static Static_object<Irq_mgr_single_chip<Irq_chip_migv>> mgr;

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  Address plic_addr = Kip::k()->platform_info.arch.plic_addr;
  Irq_mgr::mgr = mgr.construct(Kmem_mmio::remap(plic_addr, 0x1000));
}

PUBLIC static
void
Pic::handle_interrupt()
{
  mgr->c.handle_pending();
}
