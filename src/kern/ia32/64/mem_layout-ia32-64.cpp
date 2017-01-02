INTERFACE [amd64]:

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
    Utcb_addr         = 0xffff800000000000UL,    ///< % 4kB UTCB map address
    Kip_auto_map      = 0xffff800000002000UL,    ///< % 4kB
    User_max          = 0x00007fffffffffffUL,
    Service_page      = 0xffffffffeac00000UL,    ///< % 4MB global mappings
    Local_apic_page   = Service_page + 0x0000,   ///< % 4KB
    Kmem_tmp_page_1   = Service_page + 0x2000,   ///< % 4KB size 8KB
    Kmem_tmp_page_2   = Service_page + 0x4000,   ///< % 4KB size 8KB
    Tbuf_status_page  = Service_page + 0x6000,   ///< % 4KB
    Tbuf_ustatus_page = Tbuf_status_page,
    Jdb_bench_page    = Service_page + 0x8000,   ///< % 4KB
    utcb_ptr_align    = Tl_math::Ld<64>::Res,    // 64byte cachelines
    Idt               = Service_page + 0xfe000,  ///< % 4KB
    Syscalls          = Service_page + 0xff000,  ///< % 4KB syscall page
    Tbuf_buffer_area  = Service_page + 0x200000, ///< % 2MB
    Tbuf_ubuffer_area = Tbuf_buffer_area,
    // 0xffffffffeb800000-0xfffffffffec000000 (8MB) free
    Io_map_area_start = 0xffffffffec000000UL,
    Io_map_area_end   = 0xffffffffec800000UL,
    ___free_3         = 0xffffffffec800000UL, ///< % 4MB
    ___free_4         = 0xffffffffec880000UL, ///< % 4MB
    Jdb_debug_start   = 0xffffffffecc00000UL,    ///< % 4MB   JDB symbols/lines
    Jdb_debug_end     = 0xffffffffee000000UL,    ///< % 4MB
    // 0xffffffffee000000-0xffffffffef800000 (24MB) free
    Kstatic           = 0xffffffffef800000UL,    ///< % 4MB Io_bitmap
    Io_bitmap         = 0xffffffffefc00000UL,    ///< % 4MB
    Vmem_end          = 0xfffffffff0000000UL,

    Kernel_image        = FIASCO_IMAGE_VIRT_START,
    Kernel_image_end    = Kernel_image + Config::SUPERPAGE_SIZE,

    Adap_image           = Adap_in_kernel_image
                           ? Kernel_image
                           : Kernel_image + Config::SUPERPAGE_SIZE,

    Adap_vram_mda_beg = Adap_image + 0xb0000, ///< % 8KB video RAM MDA memory
    Adap_vram_mda_end = Adap_image + 0xb8000,
    Adap_vram_cga_beg = Adap_image + 0xb8000, ///< % 8KB video RAM CGA memory
    Adap_vram_cga_end = Adap_image + 0xc0000,

    Caps_start        = 0xfffffffff0800000UL,    ///< % 4MB
    Caps_end          = 0xfffffffffc400000UL,    ///< % 4MB
    Physmem           = 0xffffffff80000000UL,    ///< % 4MB   kernel memory
    Physmem_end       = 0xffffffffe0000000UL,    ///< % 4MB   kernel memory
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

IMPLEMENTATION [amd64]:

#include <cassert>

Address Mem_layout::physmem_offs;
Address Mem_layout::pmem_size;


PUBLIC static inline
void
Mem_layout::kphys_base (Address base)
{
  physmem_offs = (Address)Physmem - base;
}

PUBLIC static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys (Address addr)
{
  assert (in_pmem(addr));
  return addr - physmem_offs;
}

PUBLIC static inline NEEDS[<cassert>]
Address
Mem_layout::pmem_to_phys (const void *ptr)
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
  return addr >= Physmem && addr < Physmem_end;
}
