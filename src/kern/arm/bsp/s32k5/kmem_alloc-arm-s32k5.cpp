/*
 * Copyright 2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32k5]:

EXTENSION class Kmem_alloc
{
  static constexpr unsigned long Cpe_heap_region_start = 0x22000000;
  static constexpr unsigned long Cpe_heap_region_end   = 0x220FFFFF;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32k5]:

#include "kip.h"

/**
 * Constrain heap regions to CPE local SRAM.
 */
IMPLEMENT_OVERRIDE static
bool
Kmem_alloc::validate_free_region(Kip const *kip, unsigned long *start,
                                 unsigned long *end)
{
  (void)kip;

  if (*start < Cpe_heap_region_start)
    *start = Cpe_heap_region_start;

  if (*end > Cpe_heap_region_end)
    *end = Cpe_heap_region_end;

  return *start < *end;
}
