INTERFACE [arm]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    User_max             = 0x0000ff7fffffffff,
    Utcb_addr            = User_max + 1 - 0x10000,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && !cpu_virt]:

#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern : Address {
    Service_page         = 0xffff1000eac00000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_ustatus_page    = Tbuf_status_page,
    Tbuf_buffer_area	 = Service_page + 0x200000,
    Tbuf_ubuffer_area    = Tbuf_buffer_area,
    Jdb_tmp_map_area     = Service_page + 0x400000,
    Registers_map_start  = 0xffff000000000000,
    Registers_map_end    = 0xffff000040000000,
    Cache_flush_area     = 0xef000000,
    Cache_flush_area_end = 0xef100000,
    Map_base             = 0xffff000040000000,
    //Pmem_start           = 0xf0400000,
    //Pmem_end             = 0xf5000000,

    Caps_start           = 0xff8005000000,
    Caps_end             = 0xff800d000000,
    //Utcb_ptr_page        = 0xffffffffd000,
    // don't care about caches here, because arm uses a register on MP
    utcb_ptr_align       = Tl_math::Ld<sizeof(void*)>::Res,
  };

};


