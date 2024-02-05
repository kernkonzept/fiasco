INTERFACE [arm && mpu]:

EXTENSION class Context_base
{
protected:
  /**
   * Bitmap of user space regions that map ku_mem of currently active Mem_space.
   *
   * They can only be enabled on kernel exit. Likewise, they need to be
   * disabled on kernel entry. The member must be part of each context because
   * we switch between vCPU kernel and user mode and we don't want to replicate
   * the Context_space_ref logic in assembly.
   *
   * Because this member must also be updated from Mem_space itself, it cannot
   * be in Context. Otherwise a cyclic dependency would arise.
   */
  Unsigned32 _mpu_ku_mem_regions;

  /**
   * Kernel heap MPU region registers.
   *
   * They are cached in each Context to simplify the kernel entry where the
   * kernel heap region needs to be restored. Could be part of Context but we
   * want to have it adjacent to _mpu_usr_regions for cache efficiency.
   */
  Mword _mpu_prbar2, _mpu_prlar2;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mpu]:

/**
 * Load mask of ku_mem regions into current context.
 *
 * Updates the stored mask of ku_mem regions in the current context. This must
 * be used by the Mem_space code whenever a new ku_mem mapping is established
 * in the currently active context.
 */
PUBLIC inline static
void
Context_base::load_ku_mem_regions(Unsigned32 regions)
{
  Context_base *self = reinterpret_cast<Context_base*>(current());
  self->_mpu_ku_mem_regions = regions;
}
