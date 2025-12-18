/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32k5]:

EXTENSION class Amp_node
{
public:
  static constexpr unsigned Max_cores = 2;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && amp && pf_s32k5]:

#include "processor.h"

IMPLEMENT inline ALWAYS_INLINE NEEDS["processor.h"]
FIASCO_PURE Amp_phys_id
Amp_node::phys_id()
{
  // Aff0: 0..1 - core in cluster
  // Aff1: 0    - cluster in CPE, always 0
  // Aff2: 0    - CPE in SoC, always 0
  unsigned mpidr = cxx::int_value<Cpu_phys_id>(Proc::cpu_id());
  return Amp_phys_id(mpidr & 0x1);
}
