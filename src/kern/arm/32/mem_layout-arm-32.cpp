INTERFACE [arm && mmu && !kern_start_0xd && !cpu_virt]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0xbfffffffU; }
};


//---------------------------------------------------------------------------
INTERFACE [arm && mmu & kern_start_0xd]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0xcfffffffU; }
};
//---------------------------------------------------------------------------
INTERFACE [arm && (!mmu || cpu_virt)]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0xffffffffU; }
};


//---------------------------------------------------------------------------
INTERFACE [arm && mmu]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    Kern_lib_base	 = 0xffffe000U,
    Syscalls		 = 0xfffff000U,

    // Service area: 0xea000000 ... 0xeaffffff (16 MiB)
    Tbuf_status_page     = 0xea000000U,  // page-aligned
    Jdb_tmp_map_area     = 0xea200000U,  // superpage-aligned
    Tbuf_buffer_area     = 0xea800000U,  // page-aligned
    Tbuf_buffer_area_end = 0xeb000000U,  // Tbuf_buffer_size 2^n
    Tbuf_buffer_size     = Tbuf_buffer_area_end - Tbuf_buffer_area,

    Mmio_map_start       = 0xed000000U,
    Mmio_map_end         = 0xef000000U,
    Map_base             = 0xf0000000U,
    Pmem_start           = 0xf0400000U,
    Pmem_end             = 0xf8000000U,

    Ivt_base             = 0xffff0000U,
  };

  static_assert(Tbuf_buffer_size == 1UL << 23); // max 2^17 entries @ 32B
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !cpu_virt]:

#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern : Address {
    Cache_flush_area     = 0xef000000U,
    Cache_flush_area_end = 0xef100000U,

    Caps_start           = 0xf5000000U,
    Caps_end             = 0xfd000000U,
    Utcb_ptr_page        = 0xffffd000U,
  };

};
