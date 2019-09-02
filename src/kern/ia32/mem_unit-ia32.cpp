INTERFACE[ia32 || amd64]:

#include "types.h"

class Mem_unit
{
public:
  enum { Asid_invalid = 1 << 12 };
};

/** INVPCID types */
enum
{
  Invpcid_single_address = 0,           /**< Individual address */
  Invpcid_single_context = 1,           /**< Single-context */
  Invpcid_all_context_global = 2,       /**< All-context, including globals */
  Invpcid_all_context = 3,              /**< All-context */
};


IMPLEMENTATION[ia32 || amd64]:

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::make_coherent_to_pou(void const *)
{}

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::clean_dcache()
{ asm volatile ("wbinvd"); }

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::clean_dcache(void const *addr)
{ asm volatile ("clflush %0" : : "m" (*(char const *)addr)); }

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::clean_dcache(void const *start, void const *end)
{
  enum { Cl_size = 64 };
  if (((Address)end) - ((Address)start) >= 8192)
    clean_dcache();
  else
    for (char const *s = (char const *)start; s < (char const *)end;
         s += Cl_size)
      clean_dcache(s);
}

//----------------------------------------------------------------------------
IMPLEMENTATION[ia32 || (amd64 && !ia32_pcid)]:

/** Flush the whole TLB.
 */
PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush()
{
  Mword dummy;
  __asm__ __volatile__ ("mov %%cr3,%0; mov %0,%%cr3 " : "=r"(dummy) : : "memory");
}

/** Flush the whole TLB during early boot when PCID is not yet enabled.
 */
PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush_early()
{
  tlb_flush();
}

/** Flush TLB at virtual address.
 */
PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush(Address addr)
{
  __asm__ __volatile__ ("invlpg %0" : : "m" (*(char*)addr) : "memory");
}

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush_kernel(Address addr)
{
  return tlb_flush(addr);
}

//----------------------------------------------------------------------------
IMPLEMENTATION[amd64 && ia32_pcid]:

#include "context.h"

/**
 * Flush the TLB, either at a virtual address or flush all mappings associated
 * with a PCID.
 */
PRIVATE static inline ALWAYS_INLINE
void
Mem_unit::_invalidate_pcid(unsigned pcid, Address  address, unsigned type)
{
  struct
  {
    unsigned long pcid, address;
  } descriptor = { pcid, address };
  __asm__ __volatile__ ("invpcid %0, %1\n" : : "m" (descriptor),
                                               "r" ((unsigned long)type)
                                             : "memory");
}

/** Flush the whole TLB.
 */
PUBLIC static inline ALWAYS_INLINE NEEDS[Mem_unit::_invalidate_pcid]
void
Mem_unit::tlb_flush()
{
  _invalidate_pcid(0, 0, Invpcid_all_context_global);
}

/** Flush the whole TLB during early boot when PCID is not yet enabled.
 */
PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush_early()
{
  Mword dummy;
  __asm__ __volatile__ ("mov %%cr3,%0; mov %0,%%cr3 " : "=r"(dummy) : : "memory");
}

/** Flush the whole TLB for the given PCID.
 */
PUBLIC static inline ALWAYS_INLINE NEEDS[Mem_unit::_invalidate_pcid]
void
Mem_unit::tlb_flush(unsigned pcid)
{
  _invalidate_pcid(pcid, 0, Invpcid_single_context);
}

/** Flush TLB at virtual address of the given PCID.
 */
PUBLIC static inline ALWAYS_INLINE NEEDS[Mem_unit::_invalidate_pcid]
void
Mem_unit::tlb_flush(unsigned pcid, Address addr)
{
  _invalidate_pcid(pcid, addr, Invpcid_single_address);
}

/** Flush the whole TLB for the kernel PCID 0.
 */
PUBLIC static inline ALWAYS_INLINE NEEDS[Mem_unit::_invalidate_pcid]
void
Mem_unit::tlb_flush_kernel()
{
  _invalidate_pcid(0, 0, Invpcid_single_context);
}

/** Flush TLB at virtual address.
 */
PUBLIC static inline ALWAYS_INLINE NEEDS[Mem_unit::_invalidate_pcid]
void
Mem_unit::tlb_flush_kernel(Address addr)
{
  _invalidate_pcid(0, addr, Invpcid_single_address);
}
