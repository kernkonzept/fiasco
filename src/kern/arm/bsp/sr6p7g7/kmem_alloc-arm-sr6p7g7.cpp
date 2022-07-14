// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_sr6p7g7]:

EXTENSION class Kmem_alloc
{
  struct Region
  {
    unsigned long start;
    unsigned long end;
  };

  static constexpr Region cluster_ram[4] = {
    { 0x60400000U, 0x60400000U + (512U << 10) - 1U },
    { 0x60c00000U, 0x60c00000U + (384U << 10) - 1U },
    { 0x61400000U, 0x61400000U + (256U << 10) - 1U },
    { 0, 0 },
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_sr6p7g7]:

#include "kip.h"

/**
 * Constrain heap regions to cluster-local RAM.
 */
IMPLEMENT_OVERRIDE static
bool
Kmem_alloc::validate_free_region(Kip const *kip, unsigned long *start,
                                 unsigned long *end)
{
  Region const &heap_region = cluster_ram[(kip->node >> 1) & 3U];

  if (*start < heap_region.start)
    *start = heap_region.start;

  if (*end > heap_region.end)
    *end = heap_region.end;

  return *start < *end;
}
