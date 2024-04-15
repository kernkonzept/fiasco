INTERFACE [arm && !kern_start_0xd && !cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_umax : Address {
    User_max             = 0xbfffffff,
  };
};


//---------------------------------------------------------------------------
INTERFACE [arm && kern_start_0xd]:

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_umax : Address {
    User_max             = 0xcfffffff,
  };

};
//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_umax : Address {
    User_max             = 0xffffffff,
  };
};


//---------------------------------------------------------------------------
INTERFACE [arm]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    Kern_lib_base	 = 0xffffe000,
    Syscalls		 = 0xfffff000,

    Service_page         = 0xeac00000,
    Tbuf_status_page     = Service_page + 0x5000,
    Tbuf_ustatus_page    = Tbuf_status_page,
    Tbuf_buffer_area	 = Service_page + 0x200000,
    Tbuf_buffer_size	 = 0x200000,
    Tbuf_ubuffer_area    = Tbuf_buffer_area,
    Jdb_tmp_map_area     = Service_page + 0x400000,
    Registers_map_start  = 0xed000000,
    Registers_map_end    = 0xef000000,
    Map_base             = 0xf0000000,
    Pmem_start           = 0xf0400000,
    Pmem_end             = 0xf8000000,

    Ivt_base             = 0xffff0000,
  };
};

//---------------------------------------------------------------------------
INTERFACE [arm && !cpu_virt]:

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
    // don't care about caches here, because arm uses a register on MP
    utcb_ptr_align       = Tl_math::Ld<sizeof(void*)>::Res,
  };

};

//---------------------------------------------------------------------------
INTERFACE [arm && cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern : Address {
    Cache_flush_area     = 0x00000000, // dummy
  };
};

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && virt_obj_space]:

//---------------------------------
// Workaround GCC BUG 33661
// Do not use register asm ("r") in a template function, it will be ignored
//---------------------------------
PUBLIC static inline
Mword
Mem_layout::_read_special_safe(Mword const *a)
{
  // Counterpart: Thread::pagein_tcb_request()
  register Mword const *res __asm__ ("r14") = a;
  __asm__ __volatile__ (INST32("ldr") " %0, [%0]\n"
                        : "=r" (res) : "r" (res) : "cc" );
  return reinterpret_cast<Mword>(res);
}

//---------------------------------
// Workaround GCC BUG 33661
// Do not use register asm ("r") in a template function, it will be ignored
//---------------------------------
PUBLIC static inline
bool
Mem_layout::_read_special_safe(Mword const *address, Mword &v)
{
  // Counterpart: Thread::pagein_tcb_request()
  register Mword a asm("r14") = reinterpret_cast<Mword>(address);
  Mword ret;
  asm volatile ("msr cpsr_f, %[zero]          \n" // clear flags
                INST32("ldr") " %[a], [%[a]]  \n"
                "movne %[ret], #1             \n"
                "moveq %[ret], #0             \n"
                : [a] "=r" (a), [ret] "=r" (ret)
                : "0" (a), [zero] "r" (0)
                : "cc");
  v = a;
  return ret;
}
