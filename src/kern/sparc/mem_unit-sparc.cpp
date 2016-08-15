INTERFACE [sparc]:

#include "types.h"
#include "processor.h"

namespace Mmu
{
    enum ASI
    {
      Cache_miss    = 0x01,
      Sys_control   = 0x02,
      User_insn     = 0x08,
      Sv_insn       = 0x09,
      User_data     = 0x0a,
      Sv_data       = 0x0b,
      Icache_tags   = 0x0C,
      Icache_data   = 0x0D,
      Dcache_tags   = 0x0E,
      Dcache_data   = 0x0F,
      Icache_flush  = 0X10,
      Dcache_flush  = 0x11,
      Flush_context = 0x13,
      Diag_dcache   = 0x14,
      Diag_icache   = 0x15,
      Regs          = 0x19,
      Bypass        = 0x1C,
      Diagnostic    = 0x1D,
      Snoop_diag    = 0x1E
    };

    enum Registers
    {
      Control       = 0x000,
      ContextTable  = 0x100,
      ContextNumber = 0x200,
      Fault_status  = 0x300,
      Fault_address = 0x400,
    };
};

class Mem_unit { };

//------------------------------------------------------------------------------
IMPLEMENTATION[sparc]:

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::make_coherent_to_pou(void const *)
{}

//------------------------------------------------------------------------------
IMPLEMENTATION[sparc && !mp]:

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
}

/** Flush page at virtual address
 */
PUBLIC static inline
void
Mem_unit::tlb_flush(Address addr)
{
  (void)addr;
}

PUBLIC static inline
void
Mem_unit::sync()
{
}

PUBLIC static inline
void
Mem_unit::isync()
{
}

PUBLIC static inline
void
Mem_unit::context(Mword number)
{
  Proc::write_alternative<Mmu::Regs>(Mmu::ContextNumber, number);
}

PUBLIC static inline
void
Mem_unit::context_table(Address table)
{
  Proc::write_alternative<Mmu::Regs>(Mmu::ContextTable, (table >> 4) & ~0x3ul);
}

PUBLIC static inline
void
Mem_unit::mmu_enable()
{
  Mword r = Proc::read_alternative<Mmu::Regs>(Mmu::Control);
  r |= 1;
  Proc::write_alternative<Mmu::Regs>(Mmu::Control, r);
}
