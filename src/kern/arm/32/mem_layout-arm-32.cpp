INTERFACE [arm && mmu && !kern_start_0xd && !cpu_virt]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0xbfffffff; }
};


//---------------------------------------------------------------------------
INTERFACE [arm && mmu & kern_start_0xd]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0xcfffffff; }
};
//---------------------------------------------------------------------------
INTERFACE [arm && (!mmu || cpu_virt)]:

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0xffffffff; }
};


//---------------------------------------------------------------------------
INTERFACE [arm && mmu]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    Kern_lib_base	 = 0xffffe000,
    Syscalls		 = 0xfffff000,

    // Service area: 0xea000000 ... 0xeaffffff (16 MiB)
    Tbuf_status_page     = 0xea000000,  // page-aligned
    Jdb_tmp_map_area     = 0xea200000,  // superpage-aligned
    Tbuf_buffer_area     = 0xea800000,  // page-aligned
    Tbuf_buffer_area_end = 0xeb000000,  // Tbuf_buffer_size 2^n
    Tbuf_buffer_size     = Tbuf_buffer_area_end - Tbuf_buffer_area,

    Mmio_map_start       = 0xed000000,
    Mmio_map_end         = 0xef000000,
    Map_base             = 0xf0000000,
    Pmem_start           = 0xf0400000,
    Pmem_end             = 0xf8000000,

    Ivt_base             = 0xffff0000,
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
    Cache_flush_area     = 0xef000000,
    Cache_flush_area_end = 0xef100000,

    Caps_start           = 0xf5000000,
    Caps_end             = 0xfd000000,
    Utcb_ptr_page        = 0xffffd000,
  };

};
