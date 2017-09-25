#ifndef BOOT_PAGING_H
#define BOOT_PAGING_H

#include "types.h"

enum
{
  PAGE_SIZE 			= (1 << 12),
  PAGE_MASK			= (PAGE_SIZE - 1),
  SUPERPAGE_SIZE		= (1 << 22),
  SUPERPAGE_MASK		= (SUPERPAGE_SIZE - 1),
};

static inline int
superpage_aligned(Address x)
{ return (x & SUPERPAGE_MASK) == 0; }

static inline Address trunc_page(Address x)
{ return x & ~PAGE_MASK; }

static inline Address round_page(Address x)
{ return (x + PAGE_MASK) & ~PAGE_MASK; }

#endif
