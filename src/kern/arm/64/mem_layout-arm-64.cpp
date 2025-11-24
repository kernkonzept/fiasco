INTERFACE [arm && mmu && cpu_virt && !arm_pt48]: // -----------------------

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0x000000ff'ffffffff; }
};

INTERFACE [arm && mmu && cpu_virt && arm_pt48]: // ------------------------

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0x0000ffff'ffffffff; }
};

INTERFACE [arm && mmu && cpu_virt]: // ------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Virt_layout_kern : Address {
    // The following are kernel virtual addresses. Mind that kernel and user
    // space live in different address spaces! Move to the top to minimize the
    // risk of colliding with physical memory which is still mapped 1:1.
    Mmio_map_start       = 0x0000ffff'00000000,
    Mmio_map_end         = 0x0000ffff'40000000,

    Map_base             = 0x0000ffff'40000000,

    // Service area: 0x0000ffff'50000000 ... 0x0000ffff'51ffffff (32 MiB)
    Tbuf_status_page     = 0x0000ffff'50000000,  // page-aligned
    Jdb_tmp_map_area     = 0x0000ffff'50200000,  // superpage-aligned
    Tbuf_buffer_area     = 0x0000ffff'51000000,  // page-aligned
    Tbuf_buffer_area_end = 0x0000ffff'52000000,  // Tbuf_buffer_size 2^n
    Tbuf_buffer_size     = Tbuf_buffer_area_end - Tbuf_buffer_area,

    Pmem_start           = 0x0000ffff'80000000,
    Pmem_end             = 0x0000ffff'c0000000,
  };

  static_assert(Tbuf_buffer_size == 1UL << 24); // max 2^17 entries @ 128B
};

//---------------------------------------------------------------------------
INTERFACE [arm && mmu && !cpu_virt]:

#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  static constexpr Address user_max() { return 0x0000ff7f'ffffffff; }

  enum Virt_layout_kern : Address {
    // Service area: 0xffff1000'eac00000 ... 0xffff1000'ebffffff (20 MiB)
    Tbuf_status_page     = 0xffff1000'eac00000,  // page-aligned
    Jdb_tmp_map_area     = 0xffff1000'eae00000,  // superpage-aligned
    Tbuf_buffer_area     = 0xffff1000'eb000000,  // page-aligned
    Tbuf_buffer_area_end = 0xffff1000'ec000000,  // Tbuf_buffer_size 2^n
    Tbuf_buffer_size     = Tbuf_buffer_area_end - Tbuf_buffer_area,

    Mmio_map_start       = 0xffff0000'00000000,
    Mmio_map_end         = 0xffff0000'40000000,
    Pmem_start           = 0xffff0000'40000000,
    Pmem_end             = 0xffff0000'80000000,
    Map_base             = 0xffff0000'80000000,

    Caps_start           = 0xff80'05000000,
    Caps_end             = 0xff80'0d000000,
  };

  static_assert(Tbuf_buffer_size == 1UL << 24); // max 2^17 entries @ 128B
};

//--------------------------------------------------------------------------
INTERFACE [arm && !mmu]:

#include "config.h"

EXTENSION class Mem_layout
{
public:
  // Strictly speaking we would have to consult ID_AA64MMFR0_EL1.PARange but
  // what's the point when having no virtual memory?
  static constexpr Address user_max() { return 0xffffffff'ffffffff; }

  enum Virt_layout : Address {
    Map_base             = RAM_PHYS_BASE, // 1:1
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
