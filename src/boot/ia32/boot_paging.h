#ifndef BOOT_PAGING_H
#define BOOT_PAGING_H

#include "types.h"

enum
{
  PAGE_SIZE 			= (1 << 12),
  SUPERPAGE_SIZE		= (1 << 22),
};

static inline int
superpage_aligned(Address x)
{ return (x & (SUPERPAGE_SIZE - 1)) == 0; }

static inline Address trunc_page(Address x)
{ return x & ~(PAGE_SIZE - 1); }

static inline Address round_page(Address x)
{ return (x + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1); }

#endif
