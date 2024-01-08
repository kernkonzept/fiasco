INTERFACE:

#include "fpu.h"
#include "fpu_state_ptr.h"
#include "slab_cache.h"

class Ram_quota;

class Fpu_alloc : public Fpu
{
};

//------------------------------------------------------------------------
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

PROTECTED static inline
bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s,
                       Slab_cache *alloc, unsigned state_size)
{
  void *b;

  if (!(b = alloc->q_alloc(q)))
    return false;

  *offset_cast<Ram_quota **>(b, quota_offset(state_size)) = q;

  s.set((Fpu_state *)b);

  return true;
}

PROTECTED static inline
void
Fpu_alloc::free_state(Fpu_state_ptr &s, Slab_cache *alloc, unsigned state_size)
{
  if (!s.valid())
    return;

  Ram_quota *q = *offset_cast<Ram_quota **>(s.get(), quota_offset(state_size));
  alloc->q_free(q, s.get());
  s.set(nullptr);
}

//------------------------------------------------------------------------
IMPLEMENTATION [!fpu_alloc_typed]:

PUBLIC static inline
void
Fpu_alloc::init()
{}

PUBLIC static inline
bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s)
{
  if (!alloc_state(q, s, slab_alloc(), Fpu::state_size()))
    return false;

  Fpu::init_state(s.get());
  return true;
}

PUBLIC static inline
void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  return free_state(s, slab_alloc(), Fpu::state_size());
}

PUBLIC static inline
void
Fpu_alloc::ensure_compatible_state(Ram_quota *,
                                   Fpu_state_ptr &, Fpu_state_ptr const &)
{}

//------------------------------------------------------------------------
IMPLEMENTATION [fpu_alloc_typed]:

PUBLIC static inline
bool
Fpu_alloc::alloc_state(Ram_quota *q, Fpu_state_ptr &s,
                       Fpu::State_type type = Fpu::Default_state_type)
{
  if (!alloc_state(q, s, slab_alloc(type), Fpu::state_size(type)))
    return false;

  Fpu::init_state(s.get(), type);
  return true;
}

PUBLIC static inline
void
Fpu_alloc::free_state(Fpu_state_ptr &s)
{
  if (s.valid())
    {
      Fpu::State_type type = s.get()->type();
      return free_state(s, slab_alloc(type), Fpu::state_size(type));
    }
}

PUBLIC static inline
void
Fpu_alloc::ensure_compatible_state(Ram_quota *q,
                                   Fpu_state_ptr &to, Fpu_state_ptr const &from)
{
  if (to.get()->type() != from.get()->type())
  {
    Fpu_alloc::free_state(to);
    Fpu_alloc::alloc_state(q, to, from.get()->type());
  }
}
