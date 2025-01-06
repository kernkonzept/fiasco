/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2022 Stephan Gerhold <stephan@gerhold.net>
 * Copyright (C) 2022-2024 Kernkonzept GmbH.
 */

INTERFACE [arm && pic_gic && pf_qcom]:

#include "gic.h"
#include "initcalls.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_qcom && have_arm_gicv2]:

#include "boot_alloc.h"
#include "gic_v2.h"
#include "irq_mgr.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_single_chip<Gic_v2> Mgr;
  Mgr *m = new Boot_object<Mgr>(Kmem_mmio::map(Mem_layout::Gic_cpu_phys_base,
                                               Gic_cpu_v2::Size),
                                Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                               Gic_dist::Size));
  gic = &m->c;
  Irq_mgr::mgr = m;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && pf_qcom && have_arm_gicv3]:

#include "boot_alloc.h"
#include "gic_v3.h"
#include "irq_mgr_msi.h"
#include "kmem_mmio.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  auto *g =
    new Boot_object<Gic_v3>(Kmem_mmio::map(Mem_layout::Gic_dist_phys_base,
                                           Gic_dist::Size),
                            Kmem_mmio::map(Mem_layout::Gic_redist_phys_base,
                                           Mem_layout::Gic_redist_size));
  g->add_its(Kmem_mmio::map(Mem_layout::Gic_its_phys_base,
                            Mem_layout::Gic_its_size));
  gic = g;

  typedef Irq_mgr_msi<Gic_v3, Gic_msi> Mgr;
  Irq_mgr::mgr = new Boot_object<Mgr>(g, g->msi_chip());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp && pf_qcom]:

PUBLIC static
void Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
