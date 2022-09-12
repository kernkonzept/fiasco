// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z]:

EXTENSION class Kmem_alloc
{
  static constexpr unsigned long Rtu0_heap_region_start = 0x32100000;
  static constexpr unsigned long Rtu0_heap_region_end   = 0x327fffff;
  static constexpr unsigned long Rtu1_heap_region_start = 0x36100000;
  static constexpr unsigned long Rtu1_heap_region_end   = 0x367fffff;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_s32z]:

#include "kip.h"

/**
 * Constrain heap regions to cluster-local CRAM.
 */
IMPLEMENT_OVERRIDE static
bool
Kmem_alloc::validate_free_region(Kip const *kip, unsigned long *start,
                                 unsigned long *end)
{
  unsigned long heap_region_start, heap_region_end;

  if (kip->node < 4)
    {
      heap_region_start = Rtu0_heap_region_start;
      heap_region_end   = Rtu0_heap_region_end;
    }
  else
    {
      heap_region_start = Rtu1_heap_region_start;
      heap_region_end   = Rtu1_heap_region_end;
    }

  if (*start < heap_region_start)
    *start = heap_region_start;

  if (*end > heap_region_end)
    *end = heap_region_end;

  return *start < *end;
}
