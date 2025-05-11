//-------------------------------------------------------------------
IMPLEMENTATION [arm && pf_arm_virt]:

#include "boot_alloc.h"
#include "irq_mgr.h"
#include "irq_mgr_msi.h"
#include "gic_v2.h"
#include "gic_v3.h"
#include "panic.h"
#include "kmem_mmio.h"

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}

PUBLIC static FIASCO_INIT
void Pic::init()
{

  Mmio_register_block dist(Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                          Gic_dist::Size));
  /* Although we might be running on a v2 model, which is just a page, we
   * can check the v3 regs because QEMU's dist space is 0x10000 also for v2.
   * We check for v3 first because KVM's GIC DIST code provides the ID-regs
   * for v3 but not for v2 (v2's are just zero). */
  Unsigned32 pidr2_archrev = dist.read<Unsigned32>(0xffe8) & 0x0f0;
  if (pidr2_archrev == 0x30 || pidr2_archrev == 0x40)
    {
      printf(pidr2_archrev == 0x40 ? "GICv4\n" : "GICv3\n");
      auto redist = Kmem_mmio::map(Mem_layout::Gic_redist_phys_base,
                                   Mem_layout::Gic_redist_size);
      auto gic_v3 = new Boot_object<Gic_v3>(dist.get_mmio_base(), redist);

      gic_v3->add_its(Kmem_mmio::map(Mem_layout::Gic_its_phys_base,
                                     Mem_layout::Gic_its_size));

      gic = gic_v3;

      typedef Irq_mgr_msi<Gic_v3, Gic_msi> Mgr;
      Irq_mgr::mgr = new Boot_object<Mgr>(gic_v3, gic_v3->msi_chip());
    }
  else if (   (dist.read<Unsigned32>(0xfe8) & 0x0f0) == 0x20
           || (dist.read<Unsigned32>(0x8) & 0xfff00fff) == 0x4b00043b)
    /* As KVM's ID-regs report 0 for v2, check GICD_IIDR for the KVM value
     * (0x4b is the specific KVM-ID). */
    {
      printf("GICv2\n");
      typedef Irq_mgr_single_chip<Gic_v2> Mgr;
      Mgr *m = new Boot_object<Mgr>(Kmem_mmio::map(Mem_layout::Gic_cpu_phys_base,
                                                   Gic_cpu_v2::Size),
                                    dist.get_mmio_base());
      gic = &m->c;
      Irq_mgr::mgr = m;
    }
  else
    panic("GIC not found or not supported");
}
