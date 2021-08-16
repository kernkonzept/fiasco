INTERFACE [riscv]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  static constexpr Address round_superpage(Address addr)
  { return (addr + Config::SUPERPAGE_SIZE - 1) & Config::SUPERPAGE_MASK; }

  static constexpr Address trunc_superpage(Address addr)
  { return addr & Config::SUPERPAGE_MASK; }

  static constexpr Address round_page(Address addr)
  { return (addr + Config::PAGE_SIZE - 1) & Config::PAGE_MASK; }

private:
  static Address pmem_phys_offset;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include <cassert>

Address Mem_layout::pmem_phys_offset;

PUBLIC static inline
void
Mem_layout::pmem_phys_base(Address base)
{
  pmem_phys_offset = Pmem_start - base;
}

PUBLIC static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys(Address addr)
{
  assert(in_pmem(addr));
  return addr - pmem_phys_offset;
}

PUBLIC static inline
Address
Mem_layout::pmem_to_phys(const void *ptr)
{
  return pmem_to_phys(reinterpret_cast<Address>(ptr));
}

PUBLIC static inline
Address
Mem_layout::phys_to_pmem(Address addr)
{
  return addr + pmem_phys_offset;
}

PUBLIC static inline
bool
Mem_layout::in_pmem(Address addr)
{
  return addr >= Pmem_start && addr < Pmem_end;
}
