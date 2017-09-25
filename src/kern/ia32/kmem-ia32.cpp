// our own implementation of C++ memory management: disallow dynamic
// allocation (except where class-specific new/delete functions exist)
//
// more specialized memory allocation/deallocation functions follow
// below in the "Kmem" namespace

INTERFACE [ia32,amd64,ux]:

#include "globalconfig.h"
#include "initcalls.h"
#include "kip.h"
#include "mem_layout.h"

class Cpu;
class Pdir;
class Tss;

class Device_map
{
public:
  enum
  {
    Max = 16,
    Virt_base = 0x20000000,
  };

private:
  Address _map[Max];

};

/**
 * The system's base facilities for kernel-memory management.
 * The kernel memory is a singleton object.  We access it through a
 * static class interface.
 */
class Kmem : public Mem_layout
{
  friend class Device_map;
  friend class Jdb;
  friend class Jdb_dbinfo;
  friend class Jdb_kern_info_misc;
  friend class Kdb;
  friend class Profile;
  friend class Vmem_alloc;

private:
  Kmem();			// default constructors are undefined
  Kmem (const Kmem&);
  static unsigned long pmem_cpu_page, cpu_page_vm;

public:
  static Device_map dev_map;

  static void init_pageing(Cpu const &boot_cpu);
  static void init_boot_cpu(Cpu const &boot_cpu);
  static void init_app_cpu(Cpu const &cpu);
  static Mword is_kmem_page_fault(Address pfa, Mword error);
  static Mword is_ipc_page_fault(Address pfa, Mword error);
  static Mword is_io_bitmap_page_fault(Address pfa);
  static Address kcode_start();
  static Address kcode_end();
  static Address virt_to_phys(const void *addr);

};

typedef Kmem Kmem_space;


//----------------------------------------------------------------------------
INTERFACE [ia32, amd64]:

EXTENSION
class Kmem
{
  friend class Kernel_task;

public:
  static Address     user_max();

private:
  static Unsigned8   *io_bitmap_delimiter;
  static Address kphys_start, kphys_end;
  static Pdir *kdir; ///< Kernel page directory
};


//--------------------------------------------------------------------------
IMPLEMENTATION [ia32, amd64]:

#include "cpu.h"
#include "l4_types.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "panic.h"
#include "paging.h"
#include "pic.h"
#include "std_macros.h"

#include <cstdio>

PUBLIC
void
Device_map::init()
{
  for (unsigned i = 0; i < Max; ++i)
    _map[i] = ~0UL;
}

PRIVATE
unsigned
Device_map::lookup_idx(Address phys)
{
  Address p = phys & (~0UL << Config::SUPERPAGE_SHIFT);
  for (unsigned i = 0; i < Max; ++i)
    if (p == _map[i])
      return i;

  return ~0U;
}


PUBLIC
template< typename T >
T *
Device_map::lookup(T *phys)
{
  unsigned idx = lookup_idx((Address)phys);
  if (idx == ~0U)
    return (T*)~0UL;

  return (T*)((Virt_base + idx * Config::SUPERPAGE_SIZE)
      | ((Address)phys & ~(~0UL << Config::SUPERPAGE_SHIFT)));
}

PRIVATE
Address
Device_map::map(Address phys, bool /*cache*/)
{
  unsigned idx = lookup_idx(phys);
  if (idx != ~0U)
    return (Virt_base + idx * Config::SUPERPAGE_SIZE)
           | (phys & ~(~0UL << Config::SUPERPAGE_SHIFT));

  Address p = phys & (~0UL << Config::SUPERPAGE_SHIFT);
  Kmem_alloc *const alloc = Kmem_alloc::allocator();
  for (unsigned i = 0; i < Max; ++i)
    if (_map[i] == ~0UL)
      {
	Kmem::kdir->map(p,
                        Virt_addr(Virt_base + (i * Config::SUPERPAGE_SIZE)),
                        Virt_size(Config::SUPERPAGE_SIZE),
                        Pt_entry::Dirty | Pt_entry::Writable
                        | Pt_entry::Referenced,
                        Pt_entry::super_level(), false, pdir_alloc(alloc));
	_map[i] = p;

	return (Virt_base + (i * Config::SUPERPAGE_SIZE))
	       | (phys & ~(~0UL << Config::SUPERPAGE_SHIFT));
      }

  return ~0UL;
}

PUBLIC
template< typename T >
T *
Device_map::map(T *phys, bool cache = true)
{ return (T*)map((Address)phys, cache); }

PUBLIC
void
Device_map::unmap(void const *phys)
{
  unsigned idx = lookup_idx((Address)phys);
  if (idx == ~0U)
    return;

  Address v = Virt_base + (idx * Config::SUPERPAGE_SIZE);

  Kmem::kdir->unmap(Virt_addr(v), Virt_size(Config::SUPERPAGE_SIZE),
                    Pdir::Depth, false);
}


Unsigned8    *Kmem::io_bitmap_delimiter;
Address Kmem::kphys_start, Kmem::kphys_end;
Device_map Kmem::dev_map;


PUBLIC static inline
Address
Kmem::io_bitmap_delimiter_page()
{
  return reinterpret_cast<Address>(io_bitmap_delimiter);
}


/**
 * Compute physical address from a kernel-virtual address.
 * @param addr a virtual address
 * @return corresponding physical address if a mappings exists.
 *         -1 otherwise.
 */
IMPLEMENT inline NEEDS["paging.h", "std_macros.h", "mem_layout.h"]
Address
Kmem::virt_to_phys(const void *addr)
{
  Address a = reinterpret_cast<Address>(addr);

  if (EXPECT_TRUE(Mem_layout::in_pmem(a)))
    return Mem_layout::pmem_to_phys(a);

  if (EXPECT_TRUE(Mem_layout::in_kernel_image(a)))
    return a - Mem_layout::Kernel_image_offset;

  return kdir->virt_to_phys(a);
}


// Only used for initialization and kernel debugger
PUBLIC static
Address
Kmem::map_phys_page_tmp(Address phys, Mword idx)
{
  unsigned long pte = cxx::mask_lsb(phys, Pdir::page_order_for_level(Pdir::Depth));
  Address virt;

  switch (idx)
    {
    case 0:  virt = Mem_layout::Kmem_tmp_page_1; break;
    case 1:  virt = Mem_layout::Kmem_tmp_page_2; break;
    default: return ~0UL;
    }

  static unsigned long tmp_phys_pte[2] = { ~0UL, ~0UL };

  if (pte != tmp_phys_pte[idx])
    {
      // map two consecutive pages as to be able to access
      map_phys_page(phys,          virt,          false, true);
      map_phys_page(phys + 0x1000, virt + 0x1000, false, true);
      tmp_phys_pte[idx] = pte;
    }

  return virt + phys - pte;
}

PUBLIC static inline
Address Kmem::kernel_image_start()
{ return virt_to_phys(&Mem_layout::image_start) & Config::PAGE_MASK; }

IMPLEMENT inline Address Kmem::kcode_start()
{ return virt_to_phys(&Mem_layout::start) & Config::PAGE_MASK; }

IMPLEMENT inline Address Kmem::kcode_end()
{
  return (virt_to_phys(&Mem_layout::end) + Config::PAGE_SIZE)
         & Config::PAGE_MASK;
}


/** Return number of IPC slots to copy */
PUBLIC static inline NEEDS["config.h"]
unsigned
Kmem::ipc_slots()
{ return (8 << 20) / Config::SUPERPAGE_SIZE; }

IMPLEMENT inline NEEDS["mem_layout.h"]
Mword
Kmem::is_io_bitmap_page_fault(Address addr)
{
  return addr >= Mem_layout::Io_bitmap &&
	 addr <= Mem_layout::Io_bitmap + Mem_layout::Io_port_max / 8;
}

IMPLEMENT inline NEEDS["mem_layout.h"]
Mword
Kmem::is_kmem_page_fault(Address addr, Mword /*error*/)
{
  return addr > Mem_layout::User_max;
}


//
// helper functions
//

// Establish a 4k-mapping
PUBLIC static
void
Kmem::map_phys_page(Address phys, Address virt,
                    bool cached, bool global, Address *offs = 0)
{
  auto i = kdir->walk(Virt_addr(virt), Pdir::Depth, false,
                      pdir_alloc(Kmem_alloc::allocator()));
  Mword pte = phys & Config::PAGE_MASK;

  assert(i.level == Pdir::Depth);

  i.set_page(pte, Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::Dirty
                  | (cached ? 0 : (Pt_entry::Write_through | Pt_entry::Noncacheable))
                  | (global ? Pt_entry::global() : 0));
  Mem_unit::tlb_flush(virt);

  if (offs)
    *offs = phys - pte;
}


PUBLIC static
Address
Kmem::mmio_remap(Address phys)
{
  Address offs;
  Address va = alloc_io_vmem(Config::PAGE_SIZE);

  if (!va)
    return ~0UL;

  Kmem::map_phys_page(phys, va, false, true, &offs);
  return va + offs;
}

PUBLIC static FIASCO_INIT
void
Kmem::init_mmu()
{
  dev_map.init();
  Kmem_alloc *const alloc = Kmem_alloc::allocator();

  kdir = (Pdir*)alloc->alloc(Config::PAGE_SHIFT);
  memset (kdir, 0, Config::PAGE_SIZE);

  unsigned long cpu_features = Cpu::get_features();
  bool superpages = cpu_features & FEAT_PSE;

  printf("Superpages: %s\n", superpages?"yes":"no");

  Pt_entry::have_superpages(superpages);
  if (superpages)
    Cpu::set_cr4(Cpu::get_cr4() | CR4_PSE);

  if (cpu_features & FEAT_PGE)
    {
      Pt_entry::enable_global();
      Cpu::set_cr4 (Cpu::get_cr4() | CR4_PGE);
    }

  // set up the kernel mapping for physical memory.  mark all pages as
  // referenced and modified (so when touching the respective pages
  // later, we save the CPU overhead of marking the pd/pt entries like
  // this)

  // we also set up a one-to-one virt-to-phys mapping for two reasons:
  // (1) so that we switch to the new page table early and re-use the
  //     segment descriptors set up by boot_cpu.cc.  (we'll set up our
  //     own descriptors later.) we only need the first 4MB for that.
  // (2) a one-to-one phys-to-virt mapping in the kernel's page directory
  //     sometimes comes in handy (mostly useful for debugging)

  // first 4MB page
  kdir->map(0, Virt_addr(0UL), Virt_size(4 << 20),
      Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced,
      Pt_entry::super_level(), false, pdir_alloc(alloc));


  kdir->map(Mem_layout::Kernel_image_phys,
            Virt_addr(Mem_layout::Kernel_image),
            Virt_size(Config::SUPERPAGE_SIZE),
            Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced
            | Pt_entry::global(), Pt_entry::super_level(), false,
            pdir_alloc(alloc));

   if (!Mem_layout::Adap_in_kernel_image)
     kdir->map(Mem_layout::Adap_image_phys,
               Virt_addr(Mem_layout::Adap_image),
               Virt_size(Config::SUPERPAGE_SIZE),
               Pt_entry::Dirty | Pt_entry::Writable | Pt_entry::Referenced
               | Pt_entry::global(), Pt_entry::super_level(),
               false, pdir_alloc(alloc));

  // map the last 64MB of physical memory as kernel memory
  kdir->map(Mem_layout::pmem_to_phys(Mem_layout::Physmem),
            Virt_addr(Mem_layout::Physmem), Virt_size(Mem_layout::pmem_size),
            Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::global(),
            Pt_entry::super_level(), false, pdir_alloc(alloc));

  // The service page directory entry points to an universal usable
  // page table which is currently used for the Local APIC and the
  // jdb adapter page.
  assert((Mem_layout::Service_page & ~Config::SUPERPAGE_MASK) == 0);

  auto pt = kdir->walk(Virt_addr(Mem_layout::Service_page), Pdir::Depth,
                       false, pdir_alloc(alloc));

  // kernel mode should acknowledge write-protected page table entries
  Cpu::set_cr0(Cpu::get_cr0() | CR0_WP);

  // now switch to our new page table
  Cpu::set_pdbr(Mem_layout::pmem_to_phys(kdir));

  assert((Mem_layout::Io_bitmap & ~Config::SUPERPAGE_MASK) == 0);

  long cpu_page_size = 0x10 + Config::Max_num_cpus * (sizeof(Tss) + 256);

  if (cpu_page_size < Config::PAGE_SIZE)
    cpu_page_size = Config::PAGE_SIZE;

  pmem_cpu_page = Mem_layout::pmem_to_phys(alloc->unaligned_alloc(cpu_page_size));

  printf("Kmem:: cpu page at %lx (%ldBytes)\n", pmem_cpu_page, cpu_page_size);

  if (superpages
      && Config::SUPERPAGE_SIZE - (pmem_cpu_page & ~Config::SUPERPAGE_MASK) < 0x10000)
    {
      // can map as 4MB page because the cpu_page will land within a
      // 16-bit range from io_bitmap
      auto e = kdir->walk(Virt_addr(Mem_layout::Io_bitmap - Config::SUPERPAGE_SIZE),
                          Pdir::Super_level, false, pdir_alloc(alloc));

      e.set_page(pmem_cpu_page & Config::SUPERPAGE_MASK,
                 Pt_entry::Writable | Pt_entry::Referenced
                 | Pt_entry::Dirty | Pt_entry::global());

      cpu_page_vm = (pmem_cpu_page & ~Config::SUPERPAGE_MASK)
                    + (Mem_layout::Io_bitmap - Config::SUPERPAGE_SIZE);
    }
  else
    {
      unsigned i;
      for (i = 0; cpu_page_size > 0; ++i, cpu_page_size -= Config::PAGE_SIZE)
        {
          pt = kdir->walk(Virt_addr(Mem_layout::Io_bitmap - Config::PAGE_SIZE * (i+1)),
                          Pdir::Depth, false, pdir_alloc(alloc));

          pt.set_page(pmem_cpu_page + i*Config::PAGE_SIZE,
                      Pt_entry::Writable | Pt_entry::Referenced | Pt_entry::Dirty
                      | Pt_entry::global());
        }

      cpu_page_vm = Mem_layout::Io_bitmap - Config::PAGE_SIZE * i;
    }

  // the IO bitmap must be followed by one byte containing 0xff
  // if this byte is not present, then one gets page faults
  // (or general protection) when accessing the last port
  // at least on a Pentium 133.
  //
  // Therefore we write 0xff in the first byte of the cpu_page
  // and map this page behind every IO bitmap
  io_bitmap_delimiter = reinterpret_cast<Unsigned8 *>(cpu_page_vm);

  cpu_page_vm += 0x10;

  // did we really get the first byte ??
  assert((reinterpret_cast<Address>(io_bitmap_delimiter)
          & ~Config::PAGE_MASK) == 0);
  *io_bitmap_delimiter = 0xff;
}


PUBLIC static FIASCO_INIT_CPU
void
Kmem::init_cpu(Cpu &cpu)
{
  void *cpu_mem = Kmem_alloc::allocator()->unaligned_alloc(1024);
  printf("Allocate cpu_mem @ %p\n", cpu_mem);

  // now initialize the global descriptor table
  cpu.init_gdt(__alloc(&cpu_mem, Gdt::gdt_max), user_max());

  // Allocate the task segment as the last thing from cpu_page_vm
  // because with IO protection enabled the task segment includes the
  // rest of the page and the following IO bitmap (2 pages).
  //
  // Allocate additional 256 bytes for emergency stack right beneath
  // the tss. It is needed if we get an NMI or debug exception at
  // entry_sys_fast_ipc/entry_sys_fast_ipc_c/entry_sys_fast_ipc_log.
  Address tss_mem = alloc_tss(sizeof(Tss) + 256);
  assert(tss_mem + sizeof(Tss) + 256 < Mem_layout::Io_bitmap);
  tss_mem += 256;

  // this is actually tss_size + 1, including the io_bitmap_delimiter byte
  size_t tss_size;
  tss_size = Mem_layout::Io_bitmap + (Mem_layout::Io_port_max / 8) - tss_mem;

  assert(tss_size < 0x100000); // must fit into 20 Bits

  cpu.init_tss(tss_mem, tss_size);

  // force GDT... to memory before loading the registers
  asm volatile ( "" : : : "memory" );

  // set up the x86 CPU's memory model
  cpu.set_gdt();
  cpu.set_ldt(0);

  cpu.set_ds(Gdt::data_segment());
  cpu.set_es(Gdt::data_segment());
  cpu.set_ss(Gdt::gdt_data_kernel | Gdt::Selector_kernel);
  cpu.set_fs(Gdt::gdt_data_user   | Gdt::Selector_user);
  cpu.set_gs(Gdt::gdt_data_user   | Gdt::Selector_user);
  cpu.set_cs();

  // and finally initialize the TSS
  cpu.set_tss();

  init_cpu_arch(cpu, &cpu_mem);
}


//---------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

IMPLEMENT inline Address Kmem::user_max() { return ~0UL; }


//--------------------------------------------------------------------------
IMPLEMENTATION [ia32,ux,amd64]:

#include <cstdlib>
#include <cstddef>		// size_t
#include <cstring>		// memset

#include "config.h"
#include "cpu.h"
#include "gdt.h"
#include "globals.h"
#include "paging.h"
#include "regdefs.h"
#include "std_macros.h"
#include "tss.h"

// static class variables
unsigned long Kmem::pmem_cpu_page, Kmem::cpu_page_vm;
Pdir *Kmem::kdir;


static inline Address FIASCO_INIT_CPU
Kmem::__alloc(void **p, unsigned long size)
{
  Address r = ((unsigned long)*p + 0xf) & ~0xf;
  *p = (void*)(r + size);
  return r;
}

/**
 * Compute a kernel-virtual address for a physical address.
 * This function always returns virtual addresses within the
 * physical-memory region.
 * @pre addr <= highest kernel-accessible RAM address
 * @param addr a physical address
 * @return kernel-virtual address.
 */
PUBLIC static inline
void *
Kmem::phys_to_virt(Address addr)
{
  return reinterpret_cast<void *>(Mem_layout::phys_to_pmem(addr));
}

/** Allocate some bytes from a memory page */
PUBLIC static inline
Address
Kmem::alloc_tss(Address size)
{
  Address ret = cpu_page_vm;
  cpu_page_vm += (size + 0xf) & ~0xf;

  return ret;
}

/**
 * Return Global page directory.
 * This is the master copy of the kernel's page directory. Kernel-memory
 * allocations are kept here and copied to task page directories lazily
 * upon page fault.
 * @return kernel's global page directory
 */
PUBLIC static inline const Pdir* Kmem::dir() { return kdir; }


//--------------------------------------------------------------------------
IMPLEMENTATION [realmode && amd64]:

PUBLIC
static Address
Kmem::get_realmode_startup_pdbr()
{
  // for amd64 we need to make sure that our boot-up page directory is below
  // 4GB in physical memory
  static char _boot_pdir_page[Config::PAGE_SIZE] __attribute__((aligned(4096)));
  memcpy(_boot_pdir_page, dir(), sizeof(_boot_pdir_page));

  return Kmem::virt_to_phys(_boot_pdir_page);
}

//--------------------------------------------------------------------------
IMPLEMENTATION [realmode && ia32]:

PUBLIC
static Address
Kmem::get_realmode_startup_pdbr()
{
  return Mem_layout::pmem_to_phys(Kmem::dir());
}


