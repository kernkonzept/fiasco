INTERFACE:

#include "types.h"

class Mem_unit
{};


IMPLEMENTATION:

PUBLIC static inline void Mem_unit::tlb_flush () {}
PUBLIC static inline void Mem_unit::tlb_flush (Address) {}
PUBLIC static inline void Mem_unit::clean_dcache(void *) {}
PUBLIC static inline ALWAYS_INLINE void Mem_unit::make_coherent_to_pou(void const *) {}
