IMPLEMENTATION [arm && mpu]:

IMPLEMENT_OVERRIDE inline
void
Space::switchin_context(Space *from, Mem_space::Switchin_flags flags)
{
  Mem_space::switchin_context(from, flags);

  // We *always* have to load the ku_mem region mask into the new context. When
  // switching between two threads in the same task, Mem_space::make_current()
  // won't be called. Therefore we have to make sure the _mpu_ku_mem_regions
  // shadow copy in Context_base is in-sync.
  //
  // There is no need to take Mem_space::_lock here. Bits are only added to the
  // mask (which can be read atomically) and whenever the MPU registers are
  // updated, the mask is also loaded again.
  Mem_space::load_ku_mem_regions();
}
