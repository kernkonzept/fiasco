INTERFACE[ia32 || amd64]:

#include "types.h"

class Mem_unit
{
};


IMPLEMENTATION[ia32 || amd64]:

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::make_coherent_to_pou(void const *)
{}

/** Flush the whole TLB.
 */
PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush()
{
  Mword dummy;
  __asm__ __volatile__ ("mov %%cr3,%0; mov %0,%%cr3 " : "=r"(dummy) : : "memory");
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
