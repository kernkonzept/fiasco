INTERFACE [ia32]:

#include "types.h"
#include "config.h"
#include "linking.h"
#include "template_math.h"

EXTENSION class Mem_layout
{
public:
  enum Flags
  {
    /// Is the adapter memory in the kernel image super page?
    Adap_in_kernel_image = FIASCO_IMAGE_PHYS_START < Config::SUPERPAGE_SIZE,
  };

  enum
  {
    Utcb_addr         = 0xbff00000,    ///< % 4KB   UTCB map address
    Kip_auto_map      = 0xbfff2000,    ///< % 4KB
    User_max          = 0xbfffffff,
    Service_page      = 0xeac00000,    ///< % 4MB   global mappings
    Local_apic_page   = Service_page + 0x0000,   ///< % 4KB
    Kmem_tmp_page_1   = Service_page + 0x2000,   ///< % 4KB size 8KB
    Kmem_tmp_page_2   = Service_page + 0x4000,   ///< % 4KB size 8KB
    Tbuf_status_page  = Service_page + 0x6000,   ///< % 4KB
    Tbuf_ustatus_page = Tbuf_status_page,
    Jdb_bench_page    = Service_page + 0x8000,   ///< % 4KB
    Jdb_bts_area      = Service_page + 0xf000,   ///< % 4KB size 0x81000
    utcb_ptr_align    = Tl_math::Ld<64>::Res,    // 64byte cachelines
    Idt               = Service_page + 0xfe000,  ///< % 4KB
    Syscalls          = Service_page + 0xff000,  ///< % 4KB syscall page
    Tbuf_buffer_area  = Service_page + 0x200000, ///< % 2MB
    Tbuf_ubuffer_area = Tbuf_buffer_area,
    // 0xeb800000-0xec000000 (8MB) free
    Io_map_area_start = 0xec000000,
    Io_map_area_end   = 0xec800000,
    __free_4          = 0xec880000,    ///< % 4MB
    Jdb_debug_start   = 0xecc00000,    ///< % 4MB   JDB symbols/lines
    Jdb_debug_end     = 0xee000000,    ///< % 4MB
    // 0xee000000-0xef800000 (24MB) free
    Kstatic           = 0xef800000,    ///< Io_bitmap - 4MB
    Io_bitmap         = 0xefc00000,    ///< % 4MB
    Vmem_end          = 0xf0000000,

    Kernel_image        = FIASCO_IMAGE_VIRT_START, // usually 0xf0000000
    Kernel_image_end    = Kernel_image + Config::SUPERPAGE_SIZE,

    Adap_image           = Adap_in_kernel_image
                           ? Kernel_image
                           : Kernel_image + Config::SUPERPAGE_SIZE,

    Adap_vram_mda_beg = Adap_image + 0xb0000, ///< % 8KB video RAM MDA memory
    Adap_vram_mda_end = Adap_image + 0xb8000,
    Adap_vram_cga_beg = Adap_image + 0xb8000, ///< % 8KB video RAM CGA memory
    Adap_vram_cga_end = Adap_image + 0xc0000,

    Caps_start        = 0xf0800000,    ///< % 4MB
    Caps_end          = 0xf3000000,    ///< % 4MB == Caps_start + (1<<20) * 4
    Physmem           = 0xfc400000,    ///< % 4MB   kernel memory
  };

  enum Offsets
  {
    Kernel_image_offset = FIASCO_IMAGE_PHYS_OFFSET,
  };

  enum Phys_addrs
  {
    Kernel_image_phys = FIASCO_IMAGE_PHYS_START & Config::SUPERPAGE_MASK,
    Adap_image_phys   = 0,
  };

  template < typename T > static T* boot_data (T const *addr);

  static Address pmem_size;
private:
  static Address physmem_offs asm ("PHYSMEM_OFFS");
};

IMPLEMENTATION [ia32]:

#include <cassert>

Address Mem_layout::physmem_offs;
Address Mem_layout::pmem_size;


PUBLIC static inline
void
Mem_layout::kphys_base(Address base)
{
  physmem_offs = (Address)Physmem - base;
}

PUBLIC static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys(Address addr)
{
  assert (in_pmem(addr));
  return addr - physmem_offs;
}

PUBLIC static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys(const void *ptr)
{
  Address addr = reinterpret_cast<Address>(ptr);

  assert (in_pmem(addr));
  return addr - physmem_offs;
}

PUBLIC static inline
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
  return addr >= Physmem;
}
