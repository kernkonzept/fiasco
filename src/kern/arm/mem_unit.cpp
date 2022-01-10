INTERFACE [arm]:

#include "mem_layout.h"
#include "mmu.h"

class Mem_unit : public Mmu< Mem_layout::Cache_flush_area >
{
public:
  enum : Mword
  {
    Asid_kernel  = 0UL,
    Asid_invalid = ~0UL
  };

  static void tlb_flush();
  static void dtlb_flush(void *va);
  static void tlb_flush(unsigned long asid);
  static void tlb_flush(void *va, unsigned long asid);

  static void kernel_tlb_flush();
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT_DEFAULT inline NEEDS[Mem_unit::tlb_flush]
void Mem_unit::kernel_tlb_flush()
{ tlb_flush(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v7plus]:

#include "mmu.h"

PUBLIC static inline ALWAYS_INLINE NEEDS["mmu.h"]
void
Mem_unit::make_coherent_to_pou(void const *start, size_t size)
{
  // This does more than necessary: It writes back + invalidates the data cache
  // instead of cleaning. But this function shall not be used in performance
  // critical code anyway.
  Mmu::flush_cache(start, (Unsigned8 const *)start + size);
}
