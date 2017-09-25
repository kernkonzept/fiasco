INTERFACE [ppc32]:

#include "types.h"

class Mem_unit
{
};

//------------------------------------------------------------------------------
IMPLEMENTATION[ppc32]:

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::make_coherent_to_pou(void const *)
{}

//------------------------------------------------------------------------------
IMPLEMENTATION[ppc32 && !mp]:

/** Flush whole TLB
 *
 * Note: The 'tlbia' instruction is not implemented in G2 cores (causes a
 * program exception). Therefore, we use 'tlbie' by iterating through EA
 * bits [15-19] (see: G2 manual)
 */
PUBLIC static inline
void
Mem_unit::tlb_flush()
{

//  for(Address ea_dummy = 0; ea_dummy < (1UL << 17) /*bit 14*/;
//    ea_dummy += (1UL << 12) /* bit 19 */)
//  tlb_flush(ea_dummy);
//
//  asm volatile ("tlbsync");
}

/** Flush page at virtual address
 */
PUBLIC static inline
void 
Mem_unit::tlb_flush(Address addr)
{
  asm volatile ("tlbie %0" : : "r"(addr));
}

PUBLIC static inline
void
Mem_unit::sync()
{
  asm volatile ("sync");
}

PUBLIC static inline
void
Mem_unit::isync()
{
  asm volatile ("isync");
}

