INTERFACE:

#include "fpu.h"
#include "fpu_state_ptr.h"
#include "slab_cache.h"

class Ram_quota;

class Fpu_alloc : public Fpu
{
};

IMPLEMENTATION:

#include "kmem_slab.h"
#include "ram_quota.h"

static Kmem_slab _fpu_state_allocator(
  Fpu_alloc::quota_offset(Fpu::state_size()) + sizeof(Ram_quota *),
  Fpu::state_align(), "Fpu state");

PRIVATE static
Slab_cache *
Fpu_alloc::slab_alloc()
{
  return &_fpu_state_allocator;
}

PUBLIC static inline
unsigned
Fpu_alloc::quota_offset(unsigned state_size)
{
  return (state_size + alignof(Ram_quota *) - 1) & ~(alignof(Ram_quota *) - 1);
}

PUBLIC static
bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s)
{
  unsigned long sz = Fpu::state_size();
  void *b;

  if (!(b = slab_alloc()->q_alloc(q)))
    return false;

  *((Ram_quota **)((char*)b + quota_offset(sz))) = q;
  s.set((Fpu_state *)b);
  Fpu::init_state(s.get());

  return true;
}

PUBLIC static
void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  if (!s.valid())
    return;

  unsigned long sz = Fpu::state_size();
  Ram_quota *q = *((Ram_quota **)((char*)(s.get()) + quota_offset(sz)));
  slab_alloc()->q_free(q, s.get());
  s.set(nullptr);
}
