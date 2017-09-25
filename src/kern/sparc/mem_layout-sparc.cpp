//---------------------------------------------------------------------------
INTERFACE [sparc]:

#include "initcalls.h"
#include "template_math.h"

EXTENSION class Mem_layout
{

//TODO cbass: check what can be omitted
public:
  enum Phys_layout : Address
  {
    Utcb_ptr_page        = 0x3000,
    Syscalls_phys        = 0x4000,
    Tbuf_status_page     = 0x5000,
    Kernel_start         = 0x6000,   //end phys pool
    Syscalls             = 0xfffff000,

    User_max             = 0xefffffff,
    Tcbs                 = 0xc0000000,
    Utcb_addr            = User_max + 1 - 0x2000,
    utcb_ptr_align       = Tl_math::Ld<sizeof(void*)>::Res,
    Tcbs_end             = 0xe0000000,
    __free_1_start       = 0xec000000,
    __free_1_end         = 0xed000000,
    Map_base             = 0xf0000000, ///< % 80MB kernel memory
    Map_end              = 0xf5000000,
    Caps_start           = 0xf5000000,
    Caps_end             = 0xfd000000,
    Kernel_image         = 0xfd000000,
  };

  static Address Tbuf_buffer_area;
  static Address Tbuf_ubuffer_area;
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

#include "panic.h"
#include "paging.h"
#include <cstdio>

Address Mem_layout::Tbuf_buffer_area = 0;
Address Mem_layout::Tbuf_ubuffer_area = 0;

PUBLIC static
Address
Mem_layout::phys_to_pmem (Address addr)
{
  extern Mword kernel_srmmu_l1[256];
  for (unsigned i = 0xF0; i < 0xFF; ++i)
    {
      if (kernel_srmmu_l1[i] != 0)
        {
          Mword v_page = addr &  (0xFF << Pte_ptr::Pdir_shift);
          Mword entry  = (kernel_srmmu_l1[i] & Pte_ptr::Ppn_mask) << Pte_ptr::Ppn_addr_shift;
          if (entry == v_page)
            return (i << Pte_ptr::Pdir_shift) | (addr & ~(0xFF << Pte_ptr::Pdir_shift));
        }
    }
  return ~0L;
}

PUBLIC static inline
Address
Mem_layout::pmem_to_phys (Address addr)
{
  (void)addr;
  return ~0L;
}

PUBLIC static inline
Address
Mem_layout::pmem_to_phys (const void *ptr)
{
  return reinterpret_cast<Address>(ptr);
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


/* no page faults can occur, return true */
PUBLIC static inline
bool
Mem_layout::is_special_mapped(void const * /*a*/)
{
  return true;
}


IMPLEMENTATION [sparc && debug]:

#include "kip_init.h"


PUBLIC static FIASCO_INIT
void
Mem_layout::init()
{
  Mword alloc_size = 0x200000;
  unsigned long max = ~0UL;
  for (;;)
    {
      Mem_region r; r.start=2;r.end=1;// = Kip::k()->last_free(max);
      if (r.start > r.end)
        panic("Corrupt memory descscriptor in KIP...");

      if (r.start == r.end)
        panic("not enough kernel memory");

      max = r.start;
      Mword size = r.end - r.start + 1;
      if(alloc_size <= size)
	{
	  r.start += (size - alloc_size);
	  Kip::k()->add_mem_region(Mem_desc(r.start, r.end,
	                                    Mem_desc::Reserved));

	  printf("TBuf  installed at: [%08lx; %08lx] - %lu KB\n", 
	         r.start, r.end, alloc_size / 1024);

	  Tbuf_buffer_area = Tbuf_ubuffer_area = r.start;
	  break;
	}
    }

    if(!Tbuf_buffer_area)
      panic("Could not allocate trace buffer");
}

IMPLEMENTATION [sparc && !debug]:

PUBLIC static FIASCO_INIT
void
Mem_layout::init()
{}
