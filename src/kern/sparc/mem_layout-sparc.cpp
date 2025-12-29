//---------------------------------------------------------------------------
INTERFACE [sparc]:

#include "config.h"
#include "initcalls.h"
#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return _User_max; }

  enum Phys_layout : Address
  {
    Syscalls             = 0xfffff000ul,

    _User_max            = 0xbffffffful,
    Tcbs                 = 0xc0000000ul,
    Utcb_addr            = _User_max + 1 - 0x2000,
    Tcbs_end             = 0xe0000000ul,
    Utcb_ptr_page        = 0xe1000000ul,
    Tbuf_status_page     = 0xe1002000ul,
    Tbuf_buffer_area     = 0xec000000ul,
    Tbuf_ubuffer_area    = Tbuf_buffer_area,
    Tbuf_end             = 0xec200000ul,
    Mmio_map_start       = 0xed000000ul,
    Mmio_map_end         = 0xef000000ul,
    Map_base             = 0xf0000000ul, ///< % 80 MiB kernel memory
    Map_end              = 0xf5000000ul,
    Caps_start           = 0xf5000000ul,
    Caps_end             = 0xfd000000ul,
    Kernel_image         = 0xfd000000ul,

    Tbuf_buffer_size     = 0x2000000ul,
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
#include "paging.h"
#include "paging_bits.h"
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

IMPLEMENT static inline NEEDS["config.h", "paging_bits.h"]
Address
Mem_layout::phys_to_pmem(Address phys)
{
  Address virt = static_cast<Address>(__ph_to_pm[phys >> Config::SUPERPAGE_SHIFT]) << 16;

  if (!virt)
    return ~0UL;

  return virt | Super_pg::offset(phys);
}

IMPLEMENT static
Address
Mem_layout::pmem_to_phys(Address addr)
{
  printf("Mem_layout::pmem_to_phys(Address addr=%lx) is not implemented\n",
         addr);
  return ~0L;
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

//---------------------------------------------------------------------------
IMPLEMENTATION [sparc && virt_obj_space]:

#include "panic.h"

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
