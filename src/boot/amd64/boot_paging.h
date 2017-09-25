#ifndef BOOT_PAGING_H
#define BOOT_PAGING_H

#include "types.h"

enum
{
  PAGE_SIZE 			= (1 << 12),
  PAGE_MASK			= (PAGE_SIZE - 1),
  SUPERPAGE_SIZE		= (1 << 21),
  SUPERPAGE_MASK		= (SUPERPAGE_SIZE - 1),
  PD_SIZE			= (1 << 30),
  PD_MASK			= (PD_SIZE - 1),
  PDP_SIZE			= (1LL << 39),
  PDP_MASK			= (PDP_SIZE - 1),
};

static inline int
superpage_aligned(Address x)
{ return (x & SUPERPAGE_MASK) == 0; }

static inline int
pd_aligned(Address x)
{ return (x & PD_MASK) == 0; }

static inline int
pdp_aligned(Address x)
{ return (x & PDP_MASK) == 0; }

static inline Address trunc_page(Address x)
{ return x & ~PAGE_MASK; }

static inline Address round_page(Address x)
{ return (x + PAGE_MASK) & ~PAGE_MASK; }

#endif
