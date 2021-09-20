//------------------------------------------------------------------------
IMPLEMENTATION [arm_sve]:

static Static_object<Kmem_slab> _sve_state_allocator;

PUBLIC static
void
Fpu_alloc::init()
{
  _sve_state_allocator.construct(
    quota_offset(Fpu::state_size(Fpu::State_type::Sve)) + sizeof(Ram_quota *),
    Fpu::state_align(), "Sve state");
}

PRIVATE static
Slab_cache *
Fpu_alloc::slab_alloc(Fpu::State_type type)
{
  return type == Fpu::State_type::Sve ? _sve_state_allocator.get()
                                      : slab_alloc();
}
