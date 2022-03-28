INTERFACE [arm && pic_gic && pf_fvp_base_r]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_fvp_base_r]:

#include "irq_mgr_multi_chip.h"
#include "gic_v3.h"
#include "kmem.h"
#include "cpu.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<10> M;
  Mword id = Cpu::mpidr() & 0xffU;

  auto regs = Kmem::mmio_remap(Mem_layout::Gic_phys_base,
                               Mem_layout::Gic_phys_size);

  *gic = new Boot_object<Gic_v3>(regs, regs + Mem_layout::Gic_redist_offset,
                                 id == 0);

  M *m = new Boot_object<M>(1);
  m->add_chip(0, *gic, (*gic)->nr_irqs());

  *Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_fvp_base_r && mp]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  (*gic)->init_ap(cpu, resume);
}
