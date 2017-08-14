INTERFACE [mips32]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address
  {
    User_max             = 0x7fffffff,
    Utcb_addr            = User_max + 1UL - (Config::PAGE_SIZE * 4),
  };

  enum : Address
  {
    KSEG0  = 0x80000000,
    KSEG0e = 0x9fffffff,
    KSEG1  = 0xa0000000,
    KSEG1e = 0xbfffffff,

    Exception_base = 0x80000000,
  };

  static inline unsigned long round_page(unsigned long addr)
  { return (addr + Config::PAGE_SIZE - 1) & ~(Config::PAGE_SIZE-1); }
  static inline unsigned long trunc_page(unsigned long addr)
  { return addr & ~(Config::PAGE_SIZE-1); }
};


// ------------------------------------------------------------------------
IMPLEMENTATION [mips32]:

PUBLIC static inline
bool
Mem_layout::below_512mb(Address addr)
{ return !(addr & 0xe0000000); }

PUBLIC static inline
Address
Mem_layout::phys_to_pmem(Address addr)
{ return addr + KSEG0; }

PUBLIC static
Address
Mem_layout::ioremap_nocache(Address phys_addr, Address size)
{
  Address last_addr;

  /* Don't allow wraparound or zero size */
  last_addr = phys_addr + size - 1;
  if (!size || last_addr < phys_addr)
    return 0;

  /*
  * Map uncached objects in the low 512mb of address space using KSEG1.
  */
  if (below_512mb(last_addr))
    return KSEG1 + phys_addr;

  return 0;
}

PUBLIC static inline
Address
Mem_layout::pmem_to_phys(Address addr)
{ return addr - KSEG0; }

PUBLIC static inline
Address
Mem_layout::pmem_to_phys(void const *ptr)
{ return pmem_to_phys((Address)ptr); }

PUBLIC static inline
bool
Mem_layout::is_user_space(Address addr)
{ return addr < KSEG0; }

