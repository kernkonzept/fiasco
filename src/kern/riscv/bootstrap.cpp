INTERFACE [riscv]:

#include "boot_infos.h"
#include "mem_layout.h"
#include "paging.h"

// Written by the linker. See the :bstrap section in kernel.riscv.ld.
struct Bootstrap_info
{
  void (*entry)();
  void (*secondary_entry)();
  void *kip;
  Address phys_image_start;
  Address phys_image_end;
  Boot_paging_info pi;
};

extern Bootstrap_info bs_info;

class Bootstrap
{
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "assert.h"
#include "config.h"
#include "cpu.h"
#include "mem.h"
#include "mem_unit.h"
#include "sbi.h"
#include "std_macros.h"
#include "paging_bits.h"

#ifndef NDEBUG
void assert_fail(char const *expr, char const *file, unsigned int, void const *)
{
  Bootstrap::puts("Assertion failed at ");
  Bootstrap::puts(file);
  Bootstrap::puts(": ");
  Bootstrap::puts(expr);
  Bootstrap::panic("Assertion failed.");
}
#endif

Bootstrap_info FIASCO_BOOT_PAGING_INFO bs_info;

PUBLIC static inline
Address
Bootstrap::kern_to_boot(Virt_addr virt_addr)
{
  return cxx::int_value<Virt_addr>(virt_addr) -
    Mem_layout::Map_base + bs_info.phys_image_start;
}

PUBLIC static
void
Bootstrap::puts(char const *msg)
{
  while (*msg != 0)
  {
    Sbi::console_putchar(*msg);
    msg++;
  }
}

PUBLIC static FIASCO_NORETURN
void
Bootstrap::panic(char const *msg)
{
  puts("\033[1mPanic: ");
  puts(msg);
  puts("\033[m\n");

  Sbi::shutdown();
}

PUBLIC static
void
Bootstrap::init_paging()
{
  Kpdir *kdir =
    reinterpret_cast<Kpdir *>(kern_to_boot(bs_info.pi.kernel_page_directory()));
  map_kernel(bs_info.phys_image_start, bs_info.phys_image_end, kdir);

  // Enable paging
  Cpu::set_satp_unchecked(Mem_unit::Asid_boot,
    Cpu::phys_to_ppn(reinterpret_cast<Address>(kdir)));
  Mem_unit::tlb_flush();
}

PRIVATE static
void
Bootstrap::map_kernel(Address start_addr, Address end_addr, Kpdir *kdir)
{
  if (!Super_pg::aligned(start_addr))
    panic("Start of kernel image is not superpage aligned!");

  if (end_addr - start_addr > Mem_layout::Max_kernel_image_size)
    panic("Kernel image is larger than maximum kernel image size!");

  auto attr = Page::Attr::space_local(Page::Rights::RWX());
  auto size = Super_pg::round(end_addr - start_addr);

  auto alloc = bs_info.pi.alloc_phys(&kern_to_boot);
  auto mem_map = bs_info.pi.mem_map();

  // Temporary one-to-one mapping for bootstrap code
  if (!kdir->map(start_addr, Virt_addr(start_addr), Virt_size(size),
                 attr, Kpdir::Super_level, false, alloc, mem_map))
    panic("Failed to map bootstrap code.");

  // Mapping for kernel code
  static_assert(Super_pg::aligned(Mem_layout::Map_base),
                "Virtual base address of the kernel image is not "
                "superpage-aligned.");

  if (!kdir->map(start_addr, Virt_addr(Mem_layout::Map_base), Virt_size(size),
                 attr, Kpdir::Super_level, false, alloc, mem_map))
    panic("Failed to map kernel image.");

  // Provide kernel image size to kernel code
  bs_info.pi.kernel_image_size(size);
}

extern "C" void bootstrap_main()
{
  Bootstrap::init_paging();

  Mem::mp_mb();
  // Resume secondary harts
  extern Mword _secondary_wait_for_bootstrap_satp;
  extern void (*_secondary_wait_for_bootstrap_entry)();
  _secondary_wait_for_bootstrap_satp = Cpu::get_satp();
  _secondary_wait_for_bootstrap_entry = bs_info.secondary_entry;

  bs_info.entry();
}
