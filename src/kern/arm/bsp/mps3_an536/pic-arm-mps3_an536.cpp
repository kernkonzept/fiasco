INTERFACE [arm && pic_gic && pf_mps3_an536]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_mps3_an536]:

#include "amp_node.h"
#include "boot_alloc.h"
#include "gic_v3.h"
#include "irq_mgr.h"
#include "kmem.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v3> M;

  auto regs = Kmem::mmio_remap(Mem_layout::Gic_phys_base,
                               Mem_layout::Gic_phys_size);

  M *m = new Boot_object<M>(regs, regs + Mem_layout::Gic_redist_offset,
                            Amp_node::phys_id() == Amp_node::first_node());

  gic = &m->c;
  Irq_mgr::mgr = m;
}
