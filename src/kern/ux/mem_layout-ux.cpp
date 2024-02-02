INTERFACE[ux]:

#include "config.h"

EXTENSION class Mem_layout
{
public:

  enum
  {
    Host_as_size       = Config::Host_as_size,

    // keep those
    Host_as_base       = 0xc0000000, // Base for AS calculations
    Host_as_offset     = Host_as_base - Host_as_size,
  };

  static Address pmem_size;

private:
  static Address physmem_offs asm ("PHYSMEM_OFFS");
};

INTERFACE[ux-!context_4k]:

EXTENSION class Mem_layout
{
public:

  /** Virtual memory layout for 2KB kernel thread context. */
  enum : Mword
  {
    Vmem_start         = 0x20000000,
    Glibc_mmap_start   = 0x40000000,  ///<         fixed, Linux kernel spec.
    Glibc_mmap_end     = 0x50000000,  ///<         hoping that is enough
    Caps_start         = 0x58000000,
    Caps_end           = 0x5f000000,
    Service_page       = 0x5f000000,
    Idt                = 0x5f001000,
    Tbuf_status_page   = 0x5f002000,  ///< % 4KB   for jdb_tbuf
    Registers_map_start= 0x5f100000,
    Registers_map_end  = 0x5f600000,
    Tbuf_buffer_area   = 0x5f600000,  ///< % 2MB   tracebuffer
    Tbuf_buffer_size   = 0x00200000,

    Tss_start          = 0x5f800000,  ///< % 4MB   TSS
    Tss_end            = 0x60000000,

    Vmem_end           = 0x60000000,
    Physmem            = Vmem_end,    ///< % 4MB   physical memory
    Physmem_end        = 0xa0000000 - Host_as_offset,
    Physmem_max_size  = Physmem_end - Physmem,
  };
};

INTERFACE[ux-context_4k]:

EXTENSION class Mem_layout
{
public:

  /** Virtual memory layout for 4KB kernel thread context. */
  enum : Mword
  {
    Vmem_start         = 0x20000000,
    Caps_start         = 0x28000000,
    Caps_end           = 0x2f000000,
    Service_page       = 0x2f000000,
    Idt                = 0x2f001000,
    Tbuf_status_page   = 0x2f002000,  ///< % 4KB   for jdb_tbuf
    Registers_map_start= 0x2f100000,
    Registers_map_end  = 0x2f600000,
    Tbuf_buffer_area   = 0x2f600000,  ///< % 2MB   tracebuffer
    Tbuf_buffer_size   = 0x00200000,

    Tss_start          = 0x2f800000,  ///< % 4MB   TSS
    Tss_end            = 0x40000000,

    Glibc_mmap_start   = 0x40000000,  ///<         fixed, Linux kernel spec.
    Glibc_mmap_end     = 0x50000000,  ///<         hoping that is enough

    Vmem_end           = 0x90000000,
    Physmem            = Vmem_end,    ///< % 4MB   physical memory
    Physmem_end        = 0xb0000000 - Host_as_offset,
    Physmem_max_size  = Physmem_end - Physmem,
  };
};

INTERFACE[ux]:

#include "template_math.h"

EXTENSION class Mem_layout
{
public:

  /** Virtuel memory layout -- user address space. */
  enum
  {
    User_max           = 0xbfffffff - Host_as_offset,
    Tbuf_ubuffer_area  = 0xbfd00000 - Host_as_offset,  ///< % 1MB   size 2MB
    Utcb_addr          = 0xbff00000 - Host_as_offset,  ///< % 4KB   UTCB map address
    Utcb_ptr_page_user = 0xbfff0000 - Host_as_offset,  ///< % 4KB
    Trampoline_page    = 0xbfff1000 - Host_as_offset,  ///< % 4KB
    Kip_auto_map       = 0xbfff2000 - Host_as_offset,  ///< % 4KB
    Tbuf_ustatus_page  = 0xbfff3000 - Host_as_offset,  ///< % 4KB
    Kip_index          = 0xc0800000,  ///< % 4MB
    Syscalls           = 0xeacff000,  ///< % 4KB   syscall page
  };

  /** Physical memory layout. */
  enum
  {
    Kernel_start_frame        = 0x1000, // Frame 0 special-cased by roottask
    Trampoline_frame          = 0x2000, // Trampoline Page
    Utcb_ptr_frame            = 0x3000, // UTCB pointer page
    Sigstack_cpu0_start_frame = 0x4000, // Kernel Signal Altstack Start
    Multiboot_frame           = 0x10000, // Multiboot info + modules
    Sigstack_log2_size        = 15,     // 32kb signal stack size
    Sigstack_size             = 1 << Sigstack_log2_size,
    Sigstack_cpu0_end_frame   = Sigstack_cpu0_start_frame // Kernel Signal Altstack End
                                + Sigstack_size,
    Kernel_end_frame          = Sigstack_cpu0_end_frame,
  };

  enum
  {
    Utcb_ptr_page      = Physmem + Utcb_ptr_frame,
    utcb_ptr_align     = Tl_math::Ld<sizeof(void*)>::Res,
  };

  /// reflect symbols in linker script
  static const char task_sighandler_start  asm ("_task_sighandler_start");
  static const char task_sighandler_end    asm ("_task_sighandler_end");

  static Address const kernel_trampoline_page;
};

IMPLEMENTATION[ux]:

Address Mem_layout::physmem_offs;
Address Mem_layout::pmem_size;
Address const Mem_layout::kernel_trampoline_page =
              phys_to_pmem (Trampoline_frame);


PUBLIC static inline
User_ptr<Utcb> &
Mem_layout::user_utcb_ptr(Cpu_number cpu)
{
  // Allocate each CPUs utcb ptr in a different cacheline to avoid
  // false sharing.
  return reinterpret_cast<User_ptr<Utcb>*>(Utcb_ptr_page + (cxx::int_value<Cpu_number>(cpu) << utcb_ptr_align))[0];
}


PUBLIC static inline
void
Mem_layout::kphys_base (Address base)
{
  physmem_offs = Physmem - base;
}

IMPLEMENT static inline
Address
Mem_layout::pmem_to_phys (Address addr)
{
  return addr - Physmem;
}

IMPLEMENT static inline
Address
Mem_layout::phys_to_pmem (Address addr)
{
  return addr + Physmem;
}

PUBLIC static inline
Mword
Mem_layout::in_pmem (Address addr)
{
  return (addr >= Physmem) && (addr < Physmem_end);
}
