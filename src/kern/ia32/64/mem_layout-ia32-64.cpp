INTERFACE [amd64]:

#include "types.h"
#include "config.h"
#include "linking.h"
#include "template_math.h"
#include "paging_bits.h"

EXTENSION class Mem_layout
{
public:
  enum Flags
  {
    /// Is the adapter memory in the kernel image super page?
    Adap_in_kernel_image = FIASCO_IMAGE_PHYS_START < Config::SUPERPAGE_SIZE,
  };

  enum : Mword
  {
    Kentry_start      = 0xffff8100'00000000UL, ///< 512 GiB slot 258
    Kentry_cpu_page   = 0xffff817f'ffffc000UL, ///< last 16 KiB in slot 258

    Caps_start        = 0xffff8180'00800000UL,  ///< 512 GiB slot 259
    Caps_end          = 0xffff8180'0c400000UL,  ///< % 4 MiB

    Utcb_addr         = 0x0000007f'ff000000UL,  ///< % 4 KiB UTCB map address
    User_max          = 0x00007fff'ffffffffUL,

    Kglobal_area      = 0xffffffff'00000000UL,  ///< % 1 GiB to share 1 GiB tables (start)
    Kglobal_area_end  = 0xffffffff'80000000UL,  ///< % 1 GiB to share 1 GiB tables (end)

    // Service area: 0xffffffff'00000000 ... 0xffffffff'00400000 (4 MiB)
    Service_page      = Kglobal_area,            ///< % 4 MiB global mappings
    Jdb_tmp_map_page  = Service_page + 0x2000,   ///< % 4 KiB
    Tbuf_status_page  = Service_page + 0x6000,   ///< % 4 KiB
    Tbuf_buffer_area  = Service_page + 0x200000, ///< % 2 MiB
    Tbuf_buffer_size  = 0x200000,

    Tss_start         = Service_page + 0x400000, ///< % 4 MiB
    Tss_end           = Service_page + 0xc000000,

    Mmio_map_start    = Kglobal_area + 0xc000000UL,
    Mmio_map_end      = Kglobal_area_end,

    Vmem_end          = 0xffffffff'f0000000UL,

    Kernel_image        = FIASCO_IMAGE_VIRT_START,
    Kernel_image_size   = FIASCO_IMAGE_VIRT_SIZE,
    Kernel_image_end    = Kernel_image + Kernel_image_size,

    Adap_image           = Adap_in_kernel_image
                           ? Kernel_image
                           : Kernel_image + Kernel_image_size,

    Adap_vram_mda_beg = Adap_image + 0xb0000, ///< % 8 KiB video RAM MDA memory
    Adap_vram_mda_end = Adap_image + 0xb8000,
    Adap_vram_cga_beg = Adap_image + 0xb8000, ///< % 8 KiB video RAM CGA memory
    Adap_vram_cga_end = Adap_image + 0xc0000,

    // used for CPU_LOCAL_MAP only
    Kentry_cpu_pdir   = 0xffffffff'f0800000UL,

    Physmem           = 0xffffffff'10000000UL, ///< % 4 MiB kernel memory
    Physmem_end       = 0xffffffff'e0000000UL, ///< % 4 MiB kernel memory
    Physmem_max_size  = Physmem_end - Physmem,
  };

  enum Offsets
  {
    Kernel_image_offset = FIASCO_IMAGE_PHYS_OFFSET,
  };

  enum Phys_addrs
  {
    Kernel_image_phys
      = Super_pg::trunc(FIASCO_U_CONST(FIASCO_IMAGE_PHYS_START)),
    Adap_image_phys = 0,
  };

  template < typename T > static T* boot_data (T const *addr);

  static Address pmem_size;
private:
  static Address physmem_offs asm ("PHYSMEM_OFFS");
};

//-----------------------------------------------------------
INTERFACE [amd64 && kernel_isolation && !kernel_nx]:

EXTENSION class Mem_layout
{
public:
  enum : Mword
  {
    Kentry_cpu_syscall_entry = Kentry_cpu_page + 0x30
  };
};

//-----------------------------------------------------------
INTERFACE [amd64 && kernel_isolation && kernel_nx]:

EXTENSION class Mem_layout
{
public:
  enum : Mword
  {
    Kentry_cpu_page_text     = Kentry_cpu_page + Mword{Config::PAGE_SIZE},
    Kentry_cpu_syscall_entry = Kentry_cpu_page_text
  };
};

//-----------------------------------------------------------
INTERFACE [amd64 && !kernel_isolation]:

EXTENSION class Mem_layout
{
public:
  enum : Mword
  {
    Idt = Service_page + 0xfe000
  };
};

//-----------------------------------------------------------
INTERFACE [amd64 && kernel_isolation]:

EXTENSION class Mem_layout
{
public:
  enum : Mword
  {
    Idt = 0xffff817f'ffffa000UL, ///< IDT in Kentry area
  };
};

//-----------------------------------------------------------
IMPLEMENTATION [amd64]:

#include <cassert>

Address Mem_layout::physmem_offs;
Address Mem_layout::pmem_size;

PUBLIC static inline
void
Mem_layout::kphys_base (Address base)
{
  physmem_offs = Physmem - base;
}

IMPLEMENT static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys(Address addr)
{
  assert (in_pmem(addr));
  return addr - physmem_offs;
}

IMPLEMENT static inline
Address
Mem_layout::phys_to_pmem(Address addr)
{
  return addr + physmem_offs;
}

PUBLIC static inline
Mword
Mem_layout::in_kernel_image(Address addr)
{
  return addr >= Kernel_image && addr < Kernel_image_end;
}

PUBLIC static inline
Mword
Mem_layout::in_pmem(Address addr)
{
  return addr >= Physmem && addr < Physmem_end;
}
