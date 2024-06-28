INTERFACE [mips64]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address
  {
    //User_max             = 0x8000000000000000 - 1,
    User_max             = 0x0000000100000000 - 1,
    Utcb_addr            = User_max + 1UL - (Config::PAGE_SIZE * 4),
  };

  enum : Address
  {
    KSEG0  = 0xffffffff80000000,
    KSEG0e = 0xffffffff9fffffff,
    KSEG1  = 0xffffffffa0000000,
    KSEG1e = 0xffffffffbfffffff,

    Mmio_map_start = KSEG1,
    Mmio_map_end   = KSEG1e,

    Exception_base = 0xffffffff80000000,
  };
};


// ------------------------------------------------------------------------
IMPLEMENTATION [mips64]:

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

