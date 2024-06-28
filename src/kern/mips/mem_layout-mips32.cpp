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

    Mmio_map_start = KSEG1,
    Mmio_map_end   = KSEG1e,

    Exception_base = 0x80000000,
  };
};


// ------------------------------------------------------------------------
IMPLEMENTATION [mips32]:

IMPLEMENT static inline
Address
Mem_layout::phys_to_pmem(Address addr)
{ return addr + KSEG0; }

IMPLEMENT static inline
Address
Mem_layout::pmem_to_phys(Address addr)
{ return addr - KSEG0; }

PUBLIC static inline
bool
Mem_layout::is_user_space(Address addr)
{ return addr < KSEG0; }

