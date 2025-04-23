INTERFACE:

#include "types.h"

class Mmu
{
public:
  /* start address is include, end address is exclusive
   * (end = start + size) */
  static void clean_vdcache();
  static void clean_vdcache(void const *start, void const *end);
  static void flush_vcache();
  static void flush_vcache(void const *start, void const *end);
  static void flush_vdcache();
  static void flush_vdcache(void const *start, void const *end);
  static void inv_vdcache(void const *start, void const *end);

  /**
   * Clean the entire dcache.
   */
  static void clean_dcache();


  /**
   * Clean given D cache region.
   */
  static void clean_dcache(void const *va);

  /**
   * Clean given D cache region.
   */
  static void clean_dcache(void const *start, void const *end);
  /**
   * Clean and invalidate the entire cache.
   * D and I cache is cleaned and invalidated and the write buffer is
   * drained.
   */
  static void flush_cache();


  /**
   * Clean and invalidate the given cache region.
   * D and I cache are affected.
   */
  static void flush_cache(void const *start, void const *end);

  /**
   * Clean and invalidate the entire D cache.
   */
  static void flush_dcache();

  /**
   * Clean and invalidate the given D cache region.
   */
  static void flush_dcache(void const *start, void const *end);

  /**
   * Invalidate the given D cache region.
   */
  static void inv_dcache(void const *start, void const *end);

  /**
   * Switch page table and do the necessary things. 
   */
  static void switch_pdbr(Address base);
  
 // static void write_back_data_cache(bool ram = false);
 // static void write_back_data_cache(void *a);
};

//---------------------------------------------------------------------------
IMPLEMENTATION [!arm || arm_nocache]:

IMPLEMENT inline
void Mmu::flush_cache()
{}

IMPLEMENT inline
void Mmu::flush_cache(void const *, void const *)
{}

IMPLEMENT inline
void Mmu::clean_dcache()
{}

IMPLEMENT inline
void Mmu::clean_dcache(void const *, void const *)
{}

IMPLEMENT inline
void Mmu::flush_dcache()
{}

IMPLEMENT inline
void Mmu::flush_dcache(void const *, void const *)
{}

IMPLEMENT inline
void Mmu::inv_dcache(void const *, void const *)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION[!vcache]:

IMPLEMENT inline
void Mmu::flush_vcache(void const *, void const *)
{}

IMPLEMENT
void Mmu::clean_vdcache(void const *, void const *)
{}

IMPLEMENT
void Mmu::flush_vdcache(void const *, void const *)
{}

IMPLEMENT
void Mmu::inv_vdcache(void const *, void const *)
{}

IMPLEMENT
void Mmu::flush_vcache()
{}

IMPLEMENT
void Mmu::clean_vdcache()
{}

IMPLEMENT
void Mmu::flush_vdcache()
{}


//---------------------------------------------------------------------------
IMPLEMENTATION[vcache]:

IMPLEMENT inline
void Mmu::flush_vcache(void const *start, void const *end)
{ flush_cache(start, end); }

IMPLEMENT
void Mmu::clean_vdcache(void const *start, void const *end)
{ clean_dcache(start, end); }

IMPLEMENT
void Mmu::flush_vdcache(void const *start, void const *end)
{ flush_dcache(start, end); }

IMPLEMENT
void Mmu::inv_vdcache(void const *start, void const *end)
{ inv_dcache(start, end); }

IMPLEMENT
void Mmu::flush_vcache()
{ flush_cache(); }

IMPLEMENT
void Mmu::clean_vdcache()
{ clean_dcache(); }

IMPLEMENT
void Mmu::flush_vdcache()
{ flush_dcache(); }
