INTERFACE [riscv]:

#include "config.h"

EXTENSION class Mem_layout
{
private:
  static Address pmem_phys_offset;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include <cassert>

static_assert(
  (Mem_layout::Pmem_end - Mem_layout::Pmem_start) >= Config::kmem_max(),
  "Maximum kernel memory larger than pmem area.");

Address Mem_layout::pmem_phys_offset;

PUBLIC static inline
void
Mem_layout::pmem_phys_base(Address base)
{
  pmem_phys_offset = Pmem_start - base;
}

IMPLEMENT static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys(Address virt)
{
  assert(in_pmem(virt));
  return virt - pmem_phys_offset;
}

IMPLEMENT static inline
Address
Mem_layout::phys_to_pmem(Address phys)
{
  return phys + pmem_phys_offset;
}

PUBLIC static inline
bool
Mem_layout::in_pmem(Address addr)
{
  return addr >= Pmem_start && addr < Pmem_end;
}
