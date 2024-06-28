INTERFACE [arm && pic_gic && pf_fvp_base_r]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_fvp_base_r]:

#include "amp_node.h"
#include "boot_alloc.h"
#include "cpu.h"
#include "gic_v3.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v3> M;

  auto regs = Kmem_mmio::remap(Mem_layout::Gic_phys_base,
                               Mem_layout::Gic_phys_size);

  M *m = new Boot_object<M>(regs, regs + Mem_layout::Gic_redist_offset,
                            Amp_node::phys_id() == Amp_node::first_node());

  gic = &m->c;
  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_fvp_base_r && mp]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
