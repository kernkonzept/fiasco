/*
 * Copyright 2023-2025 NXP
 *
 * SPDX-License-Identifier: MIT
 */

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n && pf_s32n_rtu_0]:

EXTENSION class Kmem_alloc
{
  static constexpr unsigned long Rtu_heap_region_start = 0x39580000;
  static constexpr unsigned long Rtu_heap_region_end   = 0x39ffffff;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n && pf_s32n_rtu_1]:

EXTENSION class Kmem_alloc
{
  static constexpr unsigned long Rtu_heap_region_start = 0x3b580000;
  static constexpr unsigned long Rtu_heap_region_end   = 0x3bffffff;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n && pf_s32n_rtu_2]:

EXTENSION class Kmem_alloc
{
  static constexpr unsigned long Rtu_heap_region_start = 0x3d580000;
  static constexpr unsigned long Rtu_heap_region_end   = 0x3dffffff;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n && pf_s32n_rtu_3]:

EXTENSION class Kmem_alloc
{
  static constexpr unsigned long Rtu_heap_region_start = 0x3f580000;
  static constexpr unsigned long Rtu_heap_region_end   = 0x3fffffff;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32n]:

#include "kip.h"

/**
 * Constrain heap regions to RTU-local CRAM.
 */
IMPLEMENT_OVERRIDE static
bool
Kmem_alloc::validate_free_region(Kip const *kip, unsigned long *start,
                                 unsigned long *end)
{
  (void)kip;

  if (*start < Rtu_heap_region_start)
    *start = Rtu_heap_region_start;

  if (*end > Rtu_heap_region_end)
    *end = Rtu_heap_region_end;

  return *start < *end;
}
