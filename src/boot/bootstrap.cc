/* this code is run directly from boot.S.  our task is to setup the
   paging just enough so that L4 can run in its native address space 
   at 0xf0001000, and then start up L4.  */

#include <cassert>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "boot_cpu.h"
#include "boot_paging.h"
#include "boot_console.h"
#include "kernel_console.h"
#include "checksum.h"
#include "config.h"
#include "globalconfig.h"
#include "kip.h"
#include "kmem_alloc.h"
#include "mem_layout.h"
#include "mem_region.h"
#include "mem_unit.h"
#include "panic.h"
#include "processor.h"
#include "reset.h"
#include "simpleio.h"

struct check_sum
{
  char delimiter[16];
  Unsigned32 checksum_ro;
  Unsigned32 checksum_rw;
} check_sum = {"FIASCOCHECKSUM=", 0, 0};

extern "C" char _start[];
extern "C" char _end[];

#ifndef NDEBUG
void assert_fail(char const *expr, char const *file, unsigned int line,
                 void const *caller)
{
  panic("Assertion failed at %s:%u:%p: %s\n", file, line, caller, expr);
}
#endif

void
panic(char const *format, ...)
{
  va_list args;

  putstr("\033[1mPanic: ");
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  putstr("\033[m");

  for (;;)
    Proc::pause();
}

// test if [start1..end1-1] overlaps [start2..end2-1]
static
void
check_overlap (const char *str,
	       Address start1, Address end1, Address start2, Address end2)
{
  if ((start1 >= start2 && start1 < end2) || (end1 > start2 && end1 <= end2))
    panic("Kernel [0x%014lx,0x%014lx) overlaps %s [0x%014lx,0x%014lx).",
          start1, end1, str, start2, end2);
}

typedef void (*Start)(unsigned) FIASCO_FASTCALL;

extern "C" FIASCO_FASTCALL void bootstrap();

void
bootstrap()
{
  extern Kip my_kernel_info_page;
  Start start;

  // setup stuff for base_paging_init()
  base_cpu_setup();
  // now do base_paging_init(): sets up paging with one-to-one mapping
  base_paging_init();

  asm volatile ("" ::: "memory");

  Kip::init_global_kip(&my_kernel_info_page);
  Kconsole::init();
  Boot_console::init();
  printf("Boot: KIP @ %p\n", static_cast<void *>(Kip::k()));

  printf("Boot: Kmem_alloc::base_init();\n");
  if (!Kmem_alloc::base_init())
    panic("Could not reserve kernel memory.");

  printf("Boot: kernel memory reserved\n");

  // make sure that we did not forgot to discard an unused header section
  // (compare "objdump -p fiasco.debug")
  if (reinterpret_cast<Address>(_start) < Mem_layout::Kernel_image)
    panic("Fiasco kernel occupies memory below %014lx",
          Mem_layout::Kernel_image);

  if (reinterpret_cast<Address>(&_end) - Mem_layout::Kernel_image > Mem_layout::Kernel_image_size)
    panic("Fiasco boot system occupies more than %luMB",
          Mem_layout::Kernel_image_size / (1024 * 1024));

  base_map_physical_memory_for_kernel();

  Mem_unit::tlb_flush_early();

  start = reinterpret_cast<Start>(_start);

  Address phys_start = reinterpret_cast<Address>(_start)
                       - Mem_layout::Kernel_image
                       + Mem_layout::Kernel_image_phys;
  Address phys_end = reinterpret_cast<Address>(_end)
                     - Mem_layout::Kernel_image
                     + Mem_layout::Kernel_image_phys;
  check_overlap ("VGA/IO", phys_start, phys_end, 0xa0000, 0x100000);

  if (Checksum::get_checksum_ro() != check_sum.checksum_ro)
    panic("Read-only (text) checksum does not match.");

  if (Checksum::get_checksum_rw() != check_sum.checksum_rw)
    panic("Read-write (data) checksum does not match.");

  start(check_sum.checksum_ro);
}
