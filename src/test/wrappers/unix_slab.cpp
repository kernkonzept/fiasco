INTERFACE:

#include <cstddef>		// size_t

#include "slab_cache_anon.h"		// slab_cache_anon

class kmem_slab_t : public slab_cache_anon
{
};

IMPLEMENTATION:

#include <flux/page.h>

#include "unix_aligned_alloc.h"

// We only support slab size == PAGE_SIZE.
PUBLIC
kmem_slab_t::kmem_slab_t(unsigned elem_size, 
			 unsigned alignment)
  : slab_cache_anon(PAGE_SIZE, elem_size, alignment)
{
}

// We only support slab size == PAGE_SIZE.
PUBLIC
kmem_slab_t::kmem_slab_t(unsigned slab_size,
			 unsigned elem_size, 
			 unsigned alignment)
  : slab_cache_anon(slab_size, elem_size, alignment)
{
}

PUBLIC
kmem_slab_t::~kmem_slab_t()
{
  destroy();
}

// Callback functions called by our super class, slab_cache_anon, to
// allocate or free blocks

virtual void *
kmem_slab_t::block_alloc(unsigned long size, unsigned long alignment)
{
  return unix_aligned_alloc (size, alignment);
}

virtual void 
kmem_slab_t::block_free(void *block, unsigned long size)
{
  unix_aligned_free (block, size);
}
