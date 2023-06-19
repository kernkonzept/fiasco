INTERFACE [arm]:

#include "mem_layout.h"
#include "mmu.h"

class Mem_unit : public Mmu< Mem_layout::Cache_flush_area >
{
public:
  enum : Mword
  {
    Asid_invalid = ~0UL
  };

  /*
   * User space TLB maintenance functions. Must only be used for user tasks.
   * They guarantee that the maintenance operation has completed upon return.
   *
   * Although Fiasco itself never accesses user space page table mappings,
   * it is still necessary that the functions guarantee that the TLB maintenance
   * operations are completed upon return. This is because the MMU page table
   * walker is defined as a separate observer in the architecture. Thus a TLB
   * maintenance instruction "is only guaranteed to be finished for a PE after
   * the execution of DSB" (see "Ordering and completion of TLB maintenance
   * instructions" in ARM DDI 0487H.a). And since a DSB is not necessarily on
   * the return path to user space, the TLB maintenace functions have to ensure
   * the completion.
   *
   * Attention: It is not guaranteed that upon return the changes apply to the
   *            instruction stream! This is assumed to be implicitly ensured
   *            by the return to user space (context synchronization event).
   */

  static void tlb_flush();
  static void tlb_flush(unsigned long asid);
  static void tlb_flush(void *va, unsigned long asid);

  /*
   * Kernel TLB maintenance functions. Must only be used for Kmem::kdir
   * operations. They guarantee that the maintenance operation has completed
   * upon return.
   *
   * Attention: They do not imply any branch predictor maintenance. It is
   *            assumed that the kernel text does not change.
   */

  static void tlb_flush_kernel();
  static void tlb_flush_kernel(Address va);
};

//---------------------------------------------------------------------------
INTERFACE [arm && arm_asid16]:

EXTENSION class Mem_unit
{
public:
  enum { Asid_bits = 16 };
};

//---------------------------------------------------------------------------
INTERFACE [arm && !arm_asid16]:

EXTENSION class Mem_unit
{
public:
  enum { Asid_bits = 8 };
};

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
