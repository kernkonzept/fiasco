INTERFACE:

#include <cstddef>		// size_t
#include <flux/x86/types.h>	// vm_offset_t & friends
#include <flux/x86/multiboot.h> // multiboot_info
#include <flux/x86/paging.h>	// pd_entry_t

#include "kip.h"

//#include "config_gdt.h"

/* our own implementation of C++ memory management: disallow dynamic
   allocation (except where class-specific new/delete functions exist) */

void *operator new(size_t);
void operator delete(void *block);

// more specialized memory allocation/deallocation functions follow
// below in the "kmem" namespace

// kernel.ld definitions

extern "C" {
  extern char _start, _edata, _end;
  // extern char _physmem_1;
  // extern char _tcbs_1;
  // extern char _iobitmap_1;
  extern char _unused1_1, _unused2_1, _unused3_1, _unused4_io_1;
  extern char _ipc_window0_1, _ipc_window1_1;
}

#if 0
struct x86_gate;
struct x86_tss;
struct x86_desc;
#endif

// the kernel memory is a singleton object.  we access it through a
// static class interface.  (we would have preferred to access it like
// a class, but our method saves the overhead of always passing the
// "this" pointer on the (very small) kernel stack).

// this object is the system's page-level allocator
class kmem
{
public:
  // constants from the above kernel.ld declarations
//   static const vm_offset_t mem_phys = reinterpret_cast<vm_offset_t>(&_physmem_1);
  static const vm_offset_t mem_phys = 0;
//   static const vm_offset_t mem_tcbs = reinterpret_cast<vm_offset_t>(&_tcbs_1);
  static const vm_offset_t mem_user_max = 0xc0000000;

  static vm_offset_t io_bitmap;

  static const vm_offset_t ipc_window0 
    = reinterpret_cast<vm_offset_t>(&_ipc_window0_1);
  static const vm_offset_t ipc_window1 
    = reinterpret_cast<vm_offset_t>(&_ipc_window1_1);

//   enum { gdt_tss = GDT_TSS, 
//     gdt_code_kernel = GDT_CODE_KERNEL, gdt_data_kernel = GDT_DATA_KERNEL,
//     gdt_code_user = GDT_CODE_USER, gdt_data_user = GDT_DATA_USER, 
//     gdt_max = GDT_MAX };

// protected:
//   static pd_entry_t *kdir;	// kernel page directory
//   static pd_entry_t cpu_global;

private:
//   friend class kdb;
//   friend class jdb;
//   friend class profile;

  kmem();			// default constructors are undefined
  kmem(const kmem&);

//   static vm_offset_t mem_max, _himem;

  static const pd_entry_t flag_global = 0x200;	// l4-specific pg dir entry flag

//   static x86_gate *kidt;
//   static x86_tss *tss;
//   static x86_desc *gdt;

  static Kip *kinfo;

//   static multiboot_info kmbi;
//   static char kcmdline[256];
};

IMPLEMENTATION:

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// #include <flux/x86/tss.h>
// #include <flux/x86/seg.h>
// #include <flux/x86/proc_reg.h>
// #include <flux/x86/gate_init.h>
// #include <flux/x86/base_vm.h>


#include "globals.h"
// #include "entry.h"
// #include "irq_init.h"
// #include "config.h"

#include "unix_aligned_alloc.h"

//
// class kmem
//

// static class variables

// vm_offset_t kmem::mem_max, kmem::_himem;
// pd_entry_t *kmem::kdir;
// pd_entry_t  kmem::cpu_global;

static Kip the_kinfo =
  {
    /* magic: */ L4_KERNEL_INFO_MAGIC,
    /* version: */ 0x01004444,
    0, {0,},
    0, 0, 0, 0,
    0, 0, {0,},
    0, 0, {0,},
    0, 0, {0,},
    0, 0, 0, 0, 
// main_memory.high: mem_max,
    /* main_memory: */ { 0, 0x4000000},
// reserved0.low = trunc_page(virt_to_phys(&__crt_dummy__)),
// reserved0.high = round_page(virt_to_phys(&_end)),
    /* reserved0: */ { 0, 0},
    /* reserved1: */ { 0, 0},
    /* semi_reserved: */ { 1024 * 640, 1024 * 1024},
// Skipped rest -- rely on 0-initialization.
    /* clock: 0 */
  };

Kip *kmem::kinfo = &the_kinfo;

vm_offset_t kmem::io_bitmap 
  = reinterpret_cast<vm_offset_t>
      (unix_aligned_alloc(2 * PAGE_SIZE + 4,  2* PAGE_SIZE));

// multiboot_info kmem::kmbi;
// char kmem::kcmdline[256];

// x86_gate *kmem::kidt;
// x86_tss *kmem::tss;
// x86_desc *kmem::gdt;

// 
// kmem virtual-space related functions (similar to space_t)
// 
PUBLIC static
vm_offset_t 
kmem::virt_to_phys(const void *addr)
{
  return reinterpret_cast<vm_offset_t>(addr);
}

PUBLIC static
inline void *kmem::phys_to_virt(vm_offset_t addr) // physical to kernel-virtual
{
  return reinterpret_cast<void *>(addr);
}

// initialize a new task's page directory with the current master copy
// of kernel page tables
PUBLIC static
void kmem::dir_init(pd_entry_t *d)
{
  // Do nothing.
}

// 
// TLB flushing
// 
PUBLIC static
inline void kmem::tlb_flush(vm_offset_t addr) // flush tlb at virtual addr
{
  // Do nothing
}

PUBLIC static
inline void kmem::tlb_flush()
{
  // Do nothing
}

//
// ACCESSORS
//
PUBLIC static
inline vm_offset_t 
kmem::ipc_window(unsigned win)
{
  if (win == 0)
    return ipc_window0;

  return ipc_window1;
}

PUBLIC static
inline Kip *kmem::info() // returns the kernel info page
{
  return kinfo;
}

PUBLIC static
const pd_entry_t *kmem::dir() // returns the kernel's page directory
{
  assert (! "Cannot request kernel's page directory.");
  return 0;
  //  return kdir;
}

PUBLIC static
pd_entry_t kmem::pde_global() // returns mask for global pg dir entries
{
  // return cpu_global;
  return flag_global 
    | ((cpu.feature_flags & CPUF_PAGE_GLOBAL_EXT)
       ? INTEL_PDE_GLOBAL 
       : 0);
}

// PUBLIC static
// inline const multiboot_info *kmem::mbi()
// {
//   return &kmbi;
// }

// PUBLIC static
// inline const char *kmem::cmdline()
// {
//   return kcmdline;
// }

// PUBLIC static
// inline x86_gate *kmem::idt()
// {
//   return kidt;
// }

// PUBLIC static
// inline vm_offset_t kmem::himem()
// {
//   return _himem;
// }

// // Get a pointer to the CPUs kernel stack pointer
// PUBLIC static
// inline NEEDS [<flux/x86/tss.h>]
// vm_offset_t *kmem::kernel_esp()
// {
//   return reinterpret_cast<vm_offset_t*>(& tss->esp0);
// }
