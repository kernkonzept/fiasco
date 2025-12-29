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
  enum Phys_layout : Address {
    Sdram_phys_base = RAM_PHYS_BASE,
  };

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

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && virt_obj_space]:

PUBLIC static inline
template< typename T >
T
Mem_layout::read_special_safe(T const *a)
{
  // Counterpart: Thread::pagein_tcb_request(): In case of page fault, lr/r14
  // is set to 0 (and PSR.Z is set but not evaluated here).
  static_assert(sizeof(T) == sizeof(Mword), "wrong sized argument");
  register Mword const *res __asm__ ("r14") = reinterpret_cast<Mword const *>(a);
  __asm__ __volatile__ (INST32("ldr") " %0, [%0]\n"
                        : "=r" (res) : "r" (res) : "cc" );
  return static_cast<T>(reinterpret_cast<Mword>(res));
}

PUBLIC static inline
template< typename T >
bool
Mem_layout::read_special_safe(T const *address, T &v)
{
  // Counterpart: Thread::pagein_tcb_request(): In case of page fault, lr/r14
  // is set to 0 and PSR.Z is set.
  static_assert(sizeof(T) == sizeof(Mword), "wrong sized return type");
  register Mword a asm("r14") = reinterpret_cast<Mword>(address);
  Mword ret;
  asm volatile ("msr cpsr_f, %[zero]          \n" // clear flags
                INST32("ldr") " %[a], [%[a]]  \n"
                "movne %[ret], #1             \n"
                "moveq %[ret], #0             \n"
                : [a] "=r" (a), [ret] "=r" (ret)
                : "0" (a), [zero] "r" (0)
                : "cc");
  v = static_cast<T>(a);
  return ret;
}
