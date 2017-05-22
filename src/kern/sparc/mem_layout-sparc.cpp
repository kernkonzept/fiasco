//---------------------------------------------------------------------------
INTERFACE [sparc]:

#include "config.h"
#include "initcalls.h"
#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  enum Phys_layout : Address
  {
    Syscalls             = 0xfffff000,

    User_max             = 0xbfffffff,
    Tcbs                 = 0xc0000000,
    Utcb_addr            = User_max + 1 - 0x2000,
    utcb_ptr_align       = Tl_math::Ld<sizeof(void*)>::Res,
    Tcbs_end             = 0xe0000000,
    Utcb_ptr_page        = 0xe1000000,
    Tbuf_status_page     = 0xe1002000,
    Tbuf_buffer_area     = 0xec000000,
    Tbuf_ubuffer_area    = Tbuf_buffer_area,
    Tbuf_end             = 0xec200000,
    Registers_map_start  = 0xed000000,
    Registers_map_end    = 0xef000000,
    Map_base             = 0xf0000000, ///< % 80MB kernel memory
    Map_end              = 0xf5000000,
    Caps_start           = 0xf5000000,
    Caps_end             = 0xfd000000,
    Kernel_image         = 0xfd000000,
  };
private:
  static unsigned short __ph_to_pm[1UL << (32 - Config::SUPERPAGE_SHIFT)];
};

//---------------------------------------------------------------------------
INTERFACE [sparc]:

EXTENSION class Mem_layout
{
public:
  enum {
    Uart_base = 0x80000100,
  };
};


// ------------------------------------------------------------------------
IMPLEMENTATION [sparc]:

#include "config.h"
#include "panic.h"
#include "paging.h"
#include <cstdio>

unsigned short Mem_layout::__ph_to_pm[1 << (32 - Config::SUPERPAGE_SHIFT)];

PUBLIC static
Address
Mem_layout::phys_to_pmem_Old (Address addr)
{
  extern Mword kernel_srmmu_l1[256];
  for (unsigned i = 0xF0; i < 0xFF; ++i)
    {
      if (kernel_srmmu_l1[i] != 0)
        {
          Mword v_page = addr & (0xFF << Pte_ptr::Pdir_shift);
          Mword entry  = (kernel_srmmu_l1[i] & Pte_ptr::Ppn_mask) << Pte_ptr::Ppn_addr_shift;
          if (entry == v_page)
	    return (i << Pte_ptr::Pdir_shift) | (addr & ~(0xFF << Pte_ptr::Pdir_shift));
        }
    }
  return ~0L;
}

PUBLIC static inline NEEDS["config.h"]
Address
Mem_layout::phys_to_pmem(Address phys)
{
  Address virt = ((unsigned long)__ph_to_pm[phys >> Config::SUPERPAGE_SHIFT])
    << 16;

  if (!virt)
    return ~0UL;

  return virt | (phys & (Config::SUPERPAGE_SIZE - 1));
}

PUBLIC static
Address
Mem_layout::pmem_to_phys(Address addr)
{
  printf("Mem_layout::pmem_to_phys(Address addr=%lx) is not implemented\n",
         addr);
  return ~0L;
}

PUBLIC static inline
Address
Mem_layout::pmem_to_phys(const void *ptr)
{
  return pmem_to_phys(reinterpret_cast<Address>(ptr));
}

PUBLIC static inline ALWAYS_INLINE NEEDS["config.h"]
void
Mem_layout::add_pmem(Address phys, Address virt, unsigned long size)
{
  for (; size >= Config::SUPERPAGE_SIZE; size -= Config::SUPERPAGE_SIZE)
    {
      __ph_to_pm[phys >> Config::SUPERPAGE_SHIFT] = virt >> 16;
      phys += Config::SUPERPAGE_SIZE;
      virt += Config::SUPERPAGE_SIZE;
    }
}


PUBLIC static inline
template< typename V >
bool
Mem_layout::read_special_safe(V const * /* *address */, V &/*v*/)
{
  panic("%s not implemented", __PRETTY_FUNCTION__);
  return false;
}

PUBLIC static inline
template< typename T >
T
Mem_layout::read_special_safe(T const *a)
{
  Mword res;
  return T(res);
}
