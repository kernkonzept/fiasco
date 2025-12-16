/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

IMPLEMENTATION [arm && pic_gic && pf_s32n]:

#include "gic.h"
#include "gic_v3.h"
#include "irq_mgr_multi_chip.h"
#include "kmem_mmio.h"
#include "cpu.h"
#include "mru.h"

PUBLIC static FIASCO_INIT
void
Pic::init()
{
  typedef Irq_mgr_multi_chip<10> M;
  Mword id = Cpu::mpidr() & 0xffU;

  void *dist_mmio = Kmem_mmio::map(Mem_layout::Gic_phys_base,
                                   Mem_layout::Gic_phys_size);
  void *redist_mmio = offset_cast<void *>(dist_mmio,
                                          Mem_layout::Gic_redist_offset);

  gic = new Boot_object<Gic_v3>(dist_mmio, redist_mmio, id == 0);

  M *m = new Boot_object<M>(2);
  m->add_chip(0, gic.unwrap(), gic->nr_pins());
  if (auto *mru = Mru::create_mru(gic))
    m->add_chip(1024, mru, Mru::Nr_irqs);

  Irq_mgr::mgr = m;
}
