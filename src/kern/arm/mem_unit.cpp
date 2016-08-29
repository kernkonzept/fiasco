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

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::make_coherent_to_pou(void const *v)
{ clean_dcache(v); }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && armv8 && !hyp]:

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;

  Mem::dsb();
  asm volatile("tlbi vae1, %0"
               : : "r" (((unsigned long)va >> 12)
                        | (asid << 48)) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsb();
  asm volatile("tlbi aside1, %0" // TLBIASID
               : : "r" (asid << 48) : "memory");
}
