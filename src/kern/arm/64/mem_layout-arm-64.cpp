INTERFACE [arm && mmu && cpu_virt && !arm_pt48]: // -----------------------

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern_user_max : Address {
    User_max             = 0x000000ffffffffff,
  };
};

INTERFACE [arm && mmu && cpu_virt && arm_pt48]: // ------------------------

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern_user_max : Address {
    User_max             = 0x0000ffffffffffff,
  };
};

INTERFACE [arm && mmu && cpu_virt]: // ------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern : Address {
    // The following are kernel virtual addresses. Mind that kernel and user
    // space live in different address spaces! Move to the top to minimize the
    // risk of colliding with physical memory which is still mapped 1:1.
    Mmio_map_start       = 0x0000ffff00000000,
    Mmio_map_end         = 0x0000ffff40000000,

    Map_base             = 0x0000ffff40000000,

    // Service area: 0x0000ffff50000000 ... 0x0000ffff51ffffff (32MiB)
    Tbuf_status_page     = 0x0000ffff50000000,  // page-aligned
    Jdb_tmp_map_area     = 0x0000ffff50200000,  // superpage-aligned
    Tbuf_buffer_area     = 0x0000ffff51000000,  // page-aligned
    Tbuf_buffer_area_end = 0x0000ffff52000000,  // Tbuf_buffer_size 2^n
    Tbuf_buffer_size     = Tbuf_buffer_area_end - Tbuf_buffer_area,

    Pmem_start           = 0x0000ffff80000000,
    Pmem_end             = 0x0000ffffc0000000,
  };

  static_assert(Tbuf_buffer_size == 1UL << 24); // max 2^17 entries @ 64B
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !cpu_virt]:

#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern : Address {
    User_max             = 0x0000ff7fffffffff,

    // Service area: 0xffff1000eac00000 ... 0xffff1000ebffffff (20MiB)
    Tbuf_status_page     = 0xffff1000eac00000,  // page-aligned
    Jdb_tmp_map_area     = 0xffff1000eae00000,  // superpage-aligned
    Tbuf_buffer_area     = 0xffff1000eb000000,  // page-aligned
    Tbuf_buffer_area_end = 0xffff1000ec000000,  // Tbuf_buffer_size 2^n
    Tbuf_buffer_size     = Tbuf_buffer_area_end - Tbuf_buffer_area,

    Mmio_map_start       = 0xffff000000000000,
    Mmio_map_end         = 0xffff000040000000,
    Pmem_start           = 0xffff000040000000,
    Pmem_end             = 0xffff000080000000,
    Map_base             = 0xffff000080000000,

    Caps_start           = 0xff8005000000,
    Caps_end             = 0xff800d000000,
  };

  static_assert(Tbuf_buffer_size == 1UL << 24); // max 2^17 entries @ 64B
};

//--------------------------------------------------------------------------
INTERFACE [arm && !mmu]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  enum Virt_layout : Address {
    Map_base             = RAM_PHYS_BASE, // 1:1

    // Strictly speaking we would have to consult ID_AA64MMFR0_EL1.PARange but
    // what's the point when having no virtual memory?
    User_max             = 0xffffffffffffffff,
  };
};

//--------------------------------------------------------------------------
IMPLEMENTATION [arm && virt_obj_space]:

PUBLIC static inline
template< typename T >
T
Mem_layout::read_special_safe(T const *a)
{
  Mword res;
  // Counterpart: Thread::pagein_tcb_request(): In case of page fault, the
  // register is set to 0 (and PSR.Z is set but not evaluated here).
  static_assert(sizeof(T) == sizeof(Mword), "wrong size return type");
  __asm__ __volatile__ ("ldr %0, %1\n" : "=r" (res) : "m" (*a) : "cc" );
  return static_cast<T>(res);
}

PUBLIC static inline
template< typename T >
bool
Mem_layout::read_special_safe(T const *address, T &v)
{
  Mword ret;
  // Counterpart: Thread::pagein_tcb_request(): In case of page fault, the
  // register is set to 0 and PSR.Z is set.
  static_assert(sizeof(T) == sizeof(Mword), "wrong sized argument");
  asm volatile ("msr  nzcv, xzr      \n" // clear flags
                "mov  %[ret], #1     \n"
                "ldr  %[val], %[adr] \n"
                "b.ne 1f             \n"
                "mov  %[ret], xzr    \n"
                "1:                  \n"
                : [val] "=r" (v), [ret] "=&r" (ret)
                : [adr] "m" (*address)
                : "cc");
  return ret;
}
