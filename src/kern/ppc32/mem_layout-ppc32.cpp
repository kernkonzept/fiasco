//---------------------------------------------------------------------------
INTERFACE [ppc32]:

#include "initcalls.h"
#include "template_math.h"

EXTENSION class Mem_layout
{

//TODO cbass: check what can be omitted
public:
  static constexpr Address user_max() { return _User_max; }

  enum Phys_layout {
    Utcb_ptr_page        = 0x3000,
    Syscalls_phys        = 0x4000,
    Tbuf_status_page     = 0x5000,
    Kernel_start         = 0x6000,   //end phys pool
    Syscalls             = 0xfffff000,

    _User_max             = 0xefffffff,
    Tcbs                 = 0xc0000000,
    Utcb_addr            = _User_max + 1 - 0x2000,
    Tcbs_end             = 0xe0000000,
    Mmio_map_start       = 0xec000000,
    Mmio_map_end         = 0xee000000,
    Htab                 = 0xee000000, ///< % 32 MiB hashed pgtab
    Map_base             = 0xf0000000, ///< % 80 MiB kernel memory
    Map_end              = 0xf5000000,
    Caps_start           = 0xf5000000,
    Caps_end             = 0xfd000000,
    Kernel_image         = 0xfd000000,

    Tbuf_buffer_size     = 0x2000000,
  };

  static Address Tbuf_buffer_area;
  static Address Tbuf_ubuffer_area;
};



// ------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

Address Mem_layout::Tbuf_buffer_area = 0;
Address Mem_layout::Tbuf_ubuffer_area = 0;


//no virtual memory in kernel mode
IMPLEMENT static inline
Address
Mem_layout::phys_to_pmem (Address addr)
{
  return addr;
}

IMPLEMENT static inline
Address
Mem_layout::pmem_to_phys (Address addr)
{
  return addr;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32 && virt_obj_space]:

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
  asm volatile("lwz %0, 0(%1)\n"
               : "=r"(res) : "r"(a));
  return T(res);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32 && debug]:

#include <cstdio>
#include "kip_init.h"
#include "mem_region.h"
#include "panic.h"

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
	                                    Mem_desc::Reserved, false,
                                            Mem_desc::Reserved_heap));

	  printf("TBuf  installed at: [%08lx; %08lx] - %lu KiB\n",
	         r.start, r.end, alloc_size / 1024);

	  Tbuf_buffer_area = Tbuf_ubuffer_area = r.start;
	  break;
	}
    }

    if(!Tbuf_buffer_area)
      panic("Could not allocate trace buffer");
}

IMPLEMENTATION [ppc32 && !debug]:

PUBLIC static FIASCO_INIT
void
Mem_layout::init()
{}
