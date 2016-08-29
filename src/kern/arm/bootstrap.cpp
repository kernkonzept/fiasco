INTERFACE [arm]:

#include "mem_layout.h"

class Bootstrap
{
public:
  struct Order_t;
  struct Virt_addr_t;

  typedef cxx::int_type<unsigned, Order_t> Order;
  typedef cxx::int_type_order<Mword, Virt_addr_t, Order> Virt_addr;

  enum
  {
    Virt_ofs = Mem_layout::Sdram_phys_base - Mem_layout::Map_base,
  };

};

//---------------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

#include <cxx/cxx_int>

EXTENSION class Bootstrap
{
public:
  struct Phys_addr_t;
  typedef cxx::int_type_order<Unsigned64, Phys_addr_t, Order> Phys_addr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Phys_addr
Bootstrap::pt_entry(Phys_addr pa, bool cache, bool local)
{
  Phys_addr res = cxx::mask_lsb(pa, map_page_order()) | Phys_addr(1); // this is a block

  if (local)
    res |= Phys_addr(1 << 11); // nG flag

  if (cache)
    res |= Phys_addr(8);

  res |= Phys_addr(1 << 10); // AF
  res |= Phys_addr(3 << 8);  // Inner sharable
  return res;
}

//---------------------------------------------------------------------------
INTERFACE [arm && !arm_lpae]:

#include <cxx/cxx_int>

EXTENSION class Bootstrap
{
public:
  struct Phys_addr_t;
  typedef cxx::int_type_order<Unsigned32, Phys_addr_t, Order> Phys_addr;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

#include "paging.h"

static inline
Bootstrap::Order
Bootstrap::map_page_order() { return Order(20); }

PUBLIC static inline NEEDS["paging.h"]
Bootstrap::Phys_addr
Bootstrap::pt_entry(Phys_addr pa, bool cache, bool local)
{
  return cxx::mask_lsb(pa, map_page_order())
                | Phys_addr(cache ? Page::Section_cachable : Page::Section_no_cache)
                | Phys_addr(local ? Page::Section_local : Page::Section_global);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstddef>
#include "types.h"
#include "cpu.h"
#include "paging.h"

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Phys_addr
Bootstrap::map_page_size_phys()
{ return Phys_addr(1) << map_page_order(); }

static inline NEEDS[Bootstrap::map_page_order]
Bootstrap::Virt_addr
Bootstrap::map_page_size()
{ return Virt_addr(1) << map_page_order(); }

static inline void
Bootstrap::map_memory(void volatile *pd, Virt_addr va, Phys_addr pa,
                      bool cache, bool local)
{
  Phys_addr *const p = (Phys_addr*)pd;
  p[cxx::int_value<Virt_addr>(va >> map_page_order())]
    = pt_entry(pa, cache, local);
}


//---------------------------------------------------------------------------
IMPLEMENTATION [arm && armv5]:

static inline void Bootstrap::set_asid() {}
static inline void Bootstrap::set_ttbcr() {}

//---------------------------------------------------------------------------
INTERFACE [arm && (armv6 || armv7)]:

#include "kmem_space.h"

static inline void
Bootstrap::set_asid()
{
  asm volatile ("mcr p15, 0, %0, c13, c0, 1" : : "r" (0)); // ASID 0
}

static inline NEEDS["kmem_space.h"]
void
Bootstrap::set_ttbcr()
{
  asm volatile("mcr p15, 0, %[ttbcr], c2, c0, 2" // TTBCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
}


//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm1176_cache_alias_fix]:

PUBLIC static inline
void
Bootstrap::do_arm_1176_cache_alias_workaround() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae && !(armv7 || mpcore)]:

PUBLIC static inline
Bootstrap::Phys_addr
Bootstrap::init_paging(void *const page_dir)
{
  return Phys_addr((Mword)page_dir);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "kmem_space.h"
#include "mmu.h"
#include "globalconfig.h"

extern char bootstrap_bss_start[];
extern char bootstrap_bss_end[];
extern char __bss_start[];
extern char __bss_end[];
extern char kernel_page_directory[];

extern "C" void _start_kernel(void);
extern "C" void bootstrap_main()
{
  void *const page_dir = kernel_page_directory + Bootstrap::Virt_ofs;

  Unsigned32 tbbr = cxx::int_value<Bootstrap::Phys_addr>(Bootstrap::init_paging(page_dir))
                    | Page::Ttbr_bits;

  Bootstrap::create_initial_mappings(page_dir);

  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  Bootstrap::do_arm_1176_cache_alias_workaround();
  Bootstrap::enable_paging(tbbr);

  Bootstrap::add_initial_pmem();

  _start_kernel();

  while(1)
    ;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && !hyp]:

#include "cpu.h"

PUBLIC static inline NEEDS["cpu.h"]
void
Bootstrap::enable_paging(Mword pdir)
{
  unsigned domains = 0x55555555; // client for all domains
  unsigned control = Config::Cache_enabled
                     ? Cpu::Cp15_c1_cache_enabled : Cpu::Cp15_c1_cache_disabled;

  asm volatile("mcr p15, 0, %[ttbcr], c2, c0, 2" // TTBCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
  Mem::dsb();
  asm volatile("mcr p15, 0, r0, c8, c7, 0"); // TLBIALL
  Mem::dsb();
  asm volatile("mcr p15, 0, %[doms], c3, c0" // domains
               : : [doms]  "r" (domains));

  asm volatile("mcrr p15, 0, %[pdir], %[null], c2" // TTBR0
               : : [pdir]  "r" (pdir), [null]  "r" (0));

  asm volatile("mcr p15, 0, %[control], c1, c0" // control
               : : [control] "r" (control));
  Mem::isb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae && hyp]:

PUBLIC static inline void
Bootstrap::enable_paging(Mword pdir)
{
  asm volatile("mcr p15, 4, %[ttbcr], c2, c0, 2" // HTCR
               : : [ttbcr] "r" (Page::Ttbcr_bits));
  asm volatile("mcr p15, 4, %[hmair0], c10, c2, 0" // HMAIR0
               : : [hmair0] "r" (Page::Mair0_prrr_bits));
  asm volatile("mcrr p15, 4, %[pdir], %[null], c2" // HTBR
               : : [pdir]  "r" (pdir), [null]  "r" (0));
  Mem::dsb();
  asm volatile("mcr p15, 4, r0, c8, c7, 0" : : : "memory"); // TLBIALLH
  asm volatile("mcr p15, 4, r0, c8, c7, 4" : : : "memory"); // TLBIALLNSNH
  Mem::dsb();

  asm volatile("mcr p15, 4, %[control], c1, c0" // HSCTLR
      : : [control] "r" (1 | 2 | 4 | 32 | 0x1000));
  Mem::isb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

static inline
Bootstrap::Order
Bootstrap::map_page_order() { return Order(21); }

PUBLIC static inline NEEDS[Bootstrap::set_mair0]
Bootstrap::Phys_addr
Bootstrap::init_paging(void *const page_dir)
{
  extern char kernel_lpae_dir[];
  Phys_addr *const lpae = reinterpret_cast<Phys_addr*>(kernel_lpae_dir + Virt_ofs);

  for (unsigned i = 0; i < 4; ++i)
    lpae[i] = Phys_addr(((Address)page_dir + 0x1000 * i) | 3);;

  set_mair0(Page::Mair0_prrr_bits);

  return Phys_addr((Mword)lpae);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

#include "config.h"
#include "cpu.h"

PUBLIC static inline NEEDS["cpu.h", "config.h"]
void
Bootstrap::enable_paging(Mword pdir)
{
  unsigned domains = 0x55555555; // client for all domains
  unsigned control = Config::Cache_enabled
                     ? Cpu::Cp15_c1_cache_enabled : Cpu::Cp15_c1_cache_disabled;

  set_asid();
  set_ttbcr();
  Mem::dsb();
  asm volatile("mcr p15, 0, r0, c8, c7, 0"); // TLBIALL
  Mem::dsb();
  asm volatile("mcr p15, 0, %[doms], c3, c0" // domains
               : : [doms]  "r" (domains));

  asm volatile("mcr p15, 0, %[pdir], c2, c0" // TTBR0
               : : [pdir] "r" (pdir));

  asm volatile("mcr p15, 0, %[control], c1, c0" // control
               : : [control] "r" (control));
  Mem::isb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm1176_cache_alias_fix]:

PUBLIC static inline void
Bootstrap::do_arm_1176_cache_alias_workaround()
{
  Mword v;
  asm volatile ("mrc p15, 0, %0, c0, c0, 1   \n" : "=r" (v));
  if (v & ((1 << 23) | (1 << 11)))
    { // P bits set
      asm volatile ("mrc p15, 0, r0, c1, c0, 1   \n"
                    "orr r0, r0, #(1 << 6)       \n"
                    "mcr p15, 0, r0, c1, c0, 1   \n"
                    : : : "r0");
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae && (armv7 || mpcore)]:

#include "paging.h"

PUBLIC static inline NEEDS["paging.h"]
Bootstrap::Phys_addr
Bootstrap::init_paging(void *const page_dir)
{
  asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(Page::Mair0_prrr_bits));
  asm volatile ("mcr p15, 0, %0, c10, c2, 1" : : "r"(Page::Mair1_nmrr_bits));
  return Phys_addr((Mword)page_dir);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !hyp]:

#include "mem_layout.h"

static void
Bootstrap::leave_hyp_mode()
{
  Mword cpsr;
  asm volatile ("mrs %0, cpsr" : "=r"(cpsr));
  if ((cpsr & 0x1f) != 0x13)
    { // we have no hyp mode support so leave hyp mode
      Mword tmp, tmp2;
      asm volatile ("   mov %[tmp], sp   \n"
                    "   mov %[tmp2], lr  \n"
                    "   msr spsr, %[psr] \n"
                    "   adr r4, 1f       \n"
                    "   .inst 0xe12ef300 | 4   @ msr elr_hyp, r4 \n"
                    "   .inst 0xe160006e       @ eret \n"
                    "1: mov sp, %[tmp]   \n"
                    "   mov lr, %[tmp2]  \n"
                    : [tmp]"=&r"(tmp), [tmp2]"=&r"(tmp2)
                    : [psr]"r"((cpsr & ~0x1f) | 0xd3) : "cc", "r4");
    }
}

PUBLIC static inline NEEDS["mem_layout.h"]
void
Bootstrap::create_initial_mappings(void *const page_dir)
{
  typedef Bootstrap::Phys_addr Phys_addr;
  typedef Bootstrap::Virt_addr Virt_addr;

  leave_hyp_mode();

  Virt_addr va;
  Phys_addr pa;

  // map sdram linear from 0xf0000000
  for (va = Virt_addr(Mem_layout::Map_base), pa = Phys_addr(Mem_layout::Sdram_phys_base);
       va < Virt_addr(Mem_layout::Map_base + (4 << 20));
       va += Bootstrap::map_page_size(), pa += Bootstrap::map_page_size_phys())
    Bootstrap::map_memory(page_dir, va, pa, true, false);

  // map sdram 1:1
  for (va = Virt_addr(Mem_layout::Sdram_phys_base);
       va < Virt_addr(Mem_layout::Sdram_phys_base + (4 << 20));
       va += Bootstrap::map_page_size())
    Bootstrap::map_memory(page_dir, va, Phys_addr(cxx::int_value<Virt_addr>(va)), true, true);
}

PUBLIC static inline void
Bootstrap::add_initial_pmem()
{
  // The first 4MB of phys memory are always mapped to Map_base
  Mem_layout::add_pmem(Mem_layout::Sdram_phys_base, Mem_layout::Map_base,
                       4 << 20);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && hyp]:

#include "feature.h"
#include "kip.h"

PUBLIC static inline NEEDS["kip.h"]
void
Bootstrap::create_initial_mappings(void *const page_dir)
{
  extern char my_kernel_info_page[];
  typedef Bootstrap::Phys_addr Phys_addr;
  typedef Bootstrap::Virt_addr Virt_addr;

  Kip *kip = reinterpret_cast<Kip*>(my_kernel_info_page);
  for (auto const &md: kip->mem_descs_a())
    {
      if (!md.valid())
	{
	  const_cast<Mem_desc&>(md).type(Mem_desc::Undefined);
	  continue;
	}

      if (md.is_virtual())
	continue;

      unsigned long s = md.start();
      unsigned long e = md.end();

      // Sweep out stupid descriptors (that have the end before the start)
      Virt_addr va;
      Phys_addr pa;

      switch (md.type())
	{
	case Mem_desc::Conventional:
	  if (e <= s)
	    break;
          for (va = Virt_addr(s), pa = Phys_addr(s); va < Virt_addr(e);
               va += Bootstrap::map_page_size(), pa += Bootstrap::map_page_size_phys())
            Bootstrap::map_memory(page_dir, va, pa, true, false);
	  break;
	default:
	  break;
	}
    }
}

KIP_KERNEL_FEATURE("arm:hyp");

PUBLIC static inline void
Bootstrap::add_initial_pmem()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

static inline void
Bootstrap::set_mair0(Mword v)
{ asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(v)); }

asm
(
".section .text.init,#alloc,#execinstr \n"
".global _start                        \n"
"_start:                               \n"
"     ldr sp, __init_data              \n"
"     bl	bootstrap_main         \n"

"__init_data:                          \n"
".long _stack                          \n"
".previous                             \n"
".section .bss                         \n"
".p2align 3                            \n"
"	.space	2048                   \n"
"_stack:                               \n"
".previous                             \n"
);

