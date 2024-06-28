INTERFACE [arm && mmu]:

#include "mem_layout.h"
#include "mmio_register_block.h"
#include "paging.h"
#include "config.h"
#include "kip.h"

EXTENSION class Bootstrap
{
  struct Bs_mem_map
  {
    static Address phys_to_pmem(Address a)
    { return a; }
  };

  struct Bs_alloc
  {
    Bs_alloc(void *base, Mword &free_map)
    : _p(reinterpret_cast<Address>(base)), _free_map(free_map)
    {}

    static Address to_phys(void *v)
    { return reinterpret_cast<Address>(v); }

    static bool valid() { return true; }

    void *alloc(Bytes /* size */)
    {
      // assert (size == Config::PAGE_SIZE);
      // test that size is a power of two
      // assert (((size - 1) ^ size) == (size - 1));

      int x = __builtin_ffsl(_free_map);
      if (x == 0)
        {
          __builtin_trap();
          return nullptr; // OOM
        }

      _free_map &= ~(1UL << (x - 1));

      return reinterpret_cast<void *>(_p + Config::PAGE_SIZE * (x - 1));
    }

    Address _p;
    Mword &_free_map;
  };
};

IMPLEMENTATION [arm && mmu]:

#include "paging_bits.h"

static inline
Bootstrap::Order
Bootstrap::map_page_order()
{ return Order(21); }

/**
 * Map RAM range with super pages.
 *
 * The kernel, namely add_initial_pmem(), assumes that the kernel has been
 * mapped with super pages.
 */
PRIVATE
template<typename PDIR>
static inline void
Bootstrap::map_ram_range(PDIR *kd, Bs_alloc &alloc,
                         unsigned long pstart, unsigned long pend,
                         unsigned long va_offset, Page::Kern kern)
{
  pstart = Super_pg::trunc(pstart);
  pend = Super_pg::round(pend);
  unsigned long size = pend - pstart;

  for (unsigned long i = 0; i < size; i += Config::SUPERPAGE_SIZE)
    {
      auto pte = kd->walk(::Virt_addr(pstart - va_offset + i),
                          PDIR::Super_level, false, alloc, Bs_mem_map());
      pte.set_page(Phys_mem_addr(pstart + i),
                   Page::Attr(Page::Rights::RWX(), Page::Type::Normal(), kern,
                              Page::Flags::None()));
    }
}

struct Elf64_dyn
{
  enum { Reloc = 7 /* RELA */, Reloc_count = 0x6ffffff9 /* RELACOUNT */ };

  Signed64 tag;
  union {
    Unsigned64 val;
    Unsigned64 ptr;
  };
};

struct Elf64_rela
{
  Unsigned64  offset;
  Unsigned64  info;
  Signed64    addend;

  inline void apply(unsigned long load_addr)
  {
    auto *addr = reinterpret_cast<unsigned long *>(load_addr + offset);
    *addr = load_addr + addend;
  }
};

PUBLIC static void
Bootstrap::relocate(unsigned long load_addr)
{
  Elf<Elf64_dyn, Elf64_rela>::relocate(load_addr);
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm]:

PUBLIC static inline Mword
Bootstrap::read_pfr0()
{
  Mword pfr0;
  asm volatile ("mrs %0, ID_AA64PFR0_EL1" : "=r"(pfr0));
  return pfr0;
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !arm_sve]:

#include "cpu.h"

PUBLIC static inline NEEDS["cpu.h"]
void
Bootstrap::config_feature_traps(Mword, bool leave_el3, bool leave_el2)
{
  if (Cpu::Has_el3 && leave_el3)
    // Disable traps to EL3.
    asm volatile ("msr CPTR_EL3, %0" : : "r"(0UL));

  if (leave_el2)
    // Disable traps to EL2.
    asm volatile ("msr CPTR_EL2, %0" : : "r"(Cpu::Cptr_el2_generic));
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && arm_sve]:

#include "cpu.h"

PUBLIC static inline NEEDS["cpu.h"]
void
Bootstrap::config_feature_traps(Mword pfr0, bool leave_el3, bool leave_el2)
{
  bool has_sve = ((pfr0 >> 32) & 0xf) == 1;
  if (has_sve)
    {
      if (Cpu::Has_el3 && leave_el3)
        {
          // Disable traps to EL3, including SVE traps.
          asm volatile ("msr CPTR_EL3, %0" : : "r"(Cpu::Cptr_el3_ez));

          // Allow all available SVE vector lengths.
          asm volatile (".arch_extension sve    \n"
                        "msr ZCR_EL3, %0        \n"
                        ".arch_extension nosve  \n"
                        : : "r" (0xfUL));
        }

      if (leave_el2)
        {
          // Disable traps to EL2, including SVE traps.
          asm volatile ("msr CPTR_EL2, %0" : : "r"(Cpu::Cptr_el2_generic & ~Cpu::Cptr_el2_tz));

          // Allow all available SVE vector lengths.
          asm volatile (".arch_extension sve    \n"
                        "msr ZCR_EL2, %0        \n"
                        ".arch_extension nosve  \n"
                        : : "r" (0xfUL));
        }
    }
  else
    {
      if (Cpu::Has_el3 && leave_el3)
        // Disable traps to EL3.
        asm volatile ("msr CPTR_EL3, %0" : : "r"(0UL));

      if (leave_el2)
        // Disable traps to EL2.
        asm volatile ("msr CPTR_EL2, %0" : : "r"(Cpu::Cptr_el2_generic));
    }
}

IMPLEMENTATION [arm && mmu && pic_gic && !have_arm_gicv3]:

PUBLIC static void
Bootstrap::config_gic_ns()
{
  Mmio_register_block dist(Mem_layout::Gic_dist_phys_base);
  Mmio_register_block cpu(Mem_layout::Gic_cpu_phys_base);
  unsigned n = ((dist.read<Unsigned32>(4 /*GICD_TYPER*/) & 0x1f) + 1) * 32;
  dist.write<Unsigned32>(0, 0 /*Gic::GICD_CTRL*/);

  for (unsigned i = 0; i < n / 32; ++i)
    dist.write<Unsigned32>(~0U, 0x80 + i * 4);

  cpu.write<Unsigned32>(0xff, 4 /*PMR*/);
  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();
}

IMPLEMENTATION [arm && mmu && (!pic_gic || have_arm_gicv3)]:

PUBLIC static inline void
Bootstrap::config_gic_ns() {}

IMPLEMENTATION [arm && !cpu_virt]:

#include "cpu.h"

enum : Unsigned64
{
  Hcr_default_bits = Cpu::Hcr_hcd | Cpu::Hcr_rw,
};

static inline void
switch_from_el2_to_el1()
{
  Mword tmp, tmp2;

  asm volatile ("tlbi alle1");
  asm volatile ("msr HCR_EL2, %0" : : "r"(Hcr_default_bits));
  Bootstrap::config_feature_traps(Bootstrap::read_pfr0(), false, true);

  asm volatile ("dsb sy" : : : "memory");
  // SCTLR.C might toggle, so flush cache
  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  // Ensure defined state of SCTLR_EL1
  asm volatile ("msr SCTLR_EL1, %0       \n"
                : : "r" (Cpu::Sctlr_generic & ~Cpu::Sctlr_m));

  asm volatile ("   mrs %[tmp], MIDR_EL1    \n"
                "   msr VPIDR_EL2, %[tmp]   \n"
                "   mrs %[tmp], MPIDR_EL1   \n"
                "   msr VMPIDR_EL2, %[tmp]  \n"
                "   mov %[tmp], sp          \n"
                "   msr spsr_el2, %[psr]    \n"
                "   adr %[tmp2], 1f         \n"
                "   msr elr_el2, %[tmp2]    \n"
                "   eret                    \n"
                "1: mov sp, %[tmp]          \n"
                : [tmp]"=&r"(tmp), [tmp2] "=&r"(tmp2)
                : [psr]"r"((0xfUL << 6) | 5UL)
                : "cc", "memory");
}

IMPLEMENTATION [arm && mmu && !cpu_virt]:

PUBLIC static inline NEEDS["cpu.h"]
void
Bootstrap::enable_paging(Mword)
{
  unsigned long control = Cpu::Sctlr_generic;

  Mem::dsb();
  asm volatile("tlbi vmalle1is");
  Mem::dsb();
  asm volatile("msr SCTLR_EL1, %[control]" : : [control] "r" (control));
  Mem::isb();
  asm volatile ("ic iallu; dsb nsh; isb");
}

static inline void
Bootstrap::set_mair0(Mword v)
{ asm volatile ("msr MAIR_EL1, %0" : : "r"(v)); }

static inline void
switch_from_el3_to_el1()
{
  Bootstrap::config_gic_ns();
  Mword pfr0 = Bootstrap::read_pfr0();
  if (((pfr0 >> 8) & 0xf) != 0)
    {
      // EL2 supported, set HCR (RW and HCD)
      asm volatile ("msr HCR_EL2, %0" : : "r"(Hcr_default_bits));
    }

  Bootstrap::config_feature_traps(pfr0, true, true);

  // flush all E1 TLBs
  asm volatile ("tlbi alle1");

  // drain the data and instruction caches as we might switch from
  // secure to non-secure mode and only secure mode can clear
  // caches for secure memory.
  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  // setup SCR (disable monitor completely)
  asm volatile ("msr scr_el3, %0"
                : :
                "r"(Cpu::Scr_default_bits));

  Mword sctlr_el3;
  Mword sctlr_el1;
  asm ("mrs %0, sctlr_el3" : "=r"(sctlr_el3));
  asm ("mrs %0, sctlr_el1" : "=r"(sctlr_el1));
  sctlr_el1 &= ~(Cpu::Sctlr_m | Cpu::Sctlr_a | Cpu::Sctlr_sa
                 | Cpu::Sctlr_ee | Cpu::Sctlr_wxn
                 | Cpu::Sctlr_uma);
  sctlr_el1 |= Cpu::Sctlr_i | Cpu::Sctlr_c;
  sctlr_el1 |= sctlr_el3 & Cpu::Sctlr_ee;
  asm volatile ("msr sctlr_el1, %0" : : "r"(sctlr_el1));

  Mword tmp;
  // monitor mode .. switch to el1
  asm volatile (
      "   msr spsr_el3, %[psr] \n"
      "   adr x4, 1f           \n"
      "   msr elr_el3, x4      \n"
      "   mov %[tmp], sp       \n"
      "   eret                 \n"
      "1: mov sp, %[tmp]       \n"
      : [tmp]"=&r"(tmp)
      : [psr]"r"((0xfUL << 6) | 5UL)
      : "cc", "x4");
}

static void
Bootstrap::leave_hyp_mode()
{
  Mword cel;
  asm volatile ("mrs %0, CurrentEL" : "=r"(cel));
  cel >>= 2;
  cel &= 3;

  switch (cel)
    {
    case 3:
      switch_from_el3_to_el1();
      break;

    case 2:
      switch_from_el2_to_el1();
      break;
    case 1:
    default:
      break;
    }
}

PUBLIC static Bootstrap::Phys_addr
Bootstrap::init_paging()
{
  leave_hyp_mode();
  Pdir  *ud = reinterpret_cast<Pdir *>(kern_to_boot(bs_info.pi.l0_dir));
  Kpdir *kd = reinterpret_cast<Kpdir *>(kern_to_boot(bs_info.pi.l0_vdir));


  Bs_alloc alloc(kern_to_boot(bs_info.pi.scratch), bs_info.pi.free_map);

  // force allocation of MMIO+Pmem page directory
  kd->walk(::Virt_addr(Mem_layout::Mmio_map_start), kd->Super_level, false,
                       alloc, Bs_mem_map());
  kd->walk(::Virt_addr(Mem_layout::Pmem_start), kd->Super_level, false,
                       alloc, Bs_mem_map());

  // map kernel to desired virtual address
  map_ram_range(kd, alloc, bs_info.kernel_start_phys, bs_info.kernel_end_phys,
                Virt_ofs + load_addr, Page::Kern::Global());

  set_mair0(Page::Mair0_prrr_bits);

  // Create 1:1 mapping of the kernel in the idle (user) page table. Needed by
  // Fiasco bootstrap and the mp trampoline after they enable paging, and by
  // add_initial_pmem().
  map_ram_range(ud, alloc, bs_info.kernel_start_phys, bs_info.kernel_end_phys,
                0, Page::Kern::None());

  asm volatile (
      "msr tcr_el1, %2   \n"
      "dsb sy            \n"
      "msr ttbr0_el1, %0 \n"
      "msr ttbr1_el1, %1 \n"
      "isb               \n"
      : :
      "r"(ud), "r"(kd), "r"(Page::Ttbcr_bits));

  return Phys_addr(0);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

asm
(
".section .text.init,\"ax\"            \n"
".type _start,#function                \n"
".global _start                        \n"
"_start:                               \n"
"     ldr  x9, .Lstack_offs            \n"
"     adr  x10, _start                 \n"
"     add  x9, x9, x10                 \n"
"     mov  sp, x9                      \n"
"     adrp x9, :got:_start             \n"
"     ldr  x9, [x9, #:got_lo12:_start] \n"
"     adr  x10, _start                 \n"
"     sub  x0, x10, x9                 \n"
"     bl   bootstrap_main              \n"
".p2align 3                            \n"  // running uncached -> align!
".Lstack_offs: .8byte (_stack - _start)\n"
".previous                             \n"
".section .bss                         \n"
".p2align 4                            \n"
"     .space 4096                      \n"
"_stack:                               \n"
".previous                             \n"
);

// -----------------------------------------------------------------
IMPLEMENTATION [arm && mmu && cpu_virt]:

#include "infinite_loop.h"
#include "cpu.h"

PUBLIC static inline NEEDS["cpu.h"]
void
Bootstrap::enable_paging(Mword)
{
  unsigned long control = Cpu::Sctlr_generic;

  Mem::dsb();
  asm volatile("tlbi alle2is");
  asm volatile("tlbi alle1is");
  Mem::dsb();
  asm volatile("msr SCTLR_EL2, %[control]" : : [control] "r" (control));
  Mem::isb();
  asm volatile ("ic iallu; dsb nsh; isb");
}

static inline void
Bootstrap::set_mair0(Mword v)
{ asm volatile ("msr MAIR_EL2, %0" : : "r"(v)); }

static void
Bootstrap::leave_el3()
{
  Mword cel;
  asm volatile ("mrs %0, CurrentEL" : "=r"(cel));
  cel >>= 2;
  cel &= 3;

  if (cel != 3)
    return; // not on EL3 so do nothing

  Bootstrap::config_gic_ns();

  Mword pfr0 = read_pfr0();
  if (((pfr0 >> 8) & 0xf) == 0)
    {
      // EL2 not supported, crash
      L4::infinite_loop();
    }


  Bootstrap::config_feature_traps(pfr0, true, false);

  // flush all E2 TLBs
  asm volatile ("tlbi alle2is");

  // setup SCR (disable monitor completely)
  asm volatile ("msr scr_el3, %0"
                : :
                "r"(Cpu::Scr_default_bits));

  Mword sctlr_el3;
  Mword sctlr;
  asm ("mrs %0, sctlr_el3" : "=r"(sctlr_el3));
  sctlr = Cpu::Sctlr_generic & ~Cpu::Sctlr_m;
  sctlr |= sctlr_el3 & Cpu::Sctlr_ee;
  asm volatile ("dsb sy" : : : "memory");
  // drain the data and instruction caches as we might switch from
  // secure to non-secure mode and only secure mode can clear
  // caches for secure memory.
  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  asm volatile ("msr sctlr_el2, %0" : : "r"(sctlr) : "memory");

  Mword tmp;
  // monitor mode .. switch to el2
  asm volatile (
      "   msr spsr_el3, %[psr] \n"
      "   adr x4, 1f           \n"
      "   msr elr_el3, x4      \n"
      "   mov %[tmp], sp       \n"
      "   eret                 \n"
      "1: mov sp, %[tmp]       \n"
      : [tmp]"=&r"(tmp)
      : [psr]"r"((0xfUL << 6) | 9)
      : "cc", "x4");
}

PUBLIC static Bootstrap::Phys_addr
Bootstrap::init_paging()
{
  // HCR_EL2.{E2H,TGE} change behavior of paging so make sure they are disabled
  asm volatile ("msr HCR_EL2, %0" : : "r"(Cpu::Hcr_rw));

  leave_el3();

  Kpdir *d = reinterpret_cast<Kpdir *>(kern_to_boot(bs_info.pi.l0_dir));
  set_mair0(Page::Mair0_prrr_bits);

  Bs_alloc alloc(kern_to_boot(bs_info.pi.scratch), bs_info.pi.free_map);

  // force allocation of MMIO+Pmem page directory
  d->walk(::Virt_addr(Mem_layout::Mmio_map_start), d->Super_level, false,
                      alloc, Bs_mem_map());
  d->walk(::Virt_addr(Mem_layout::Pmem_start), d->Super_level, false,
                      alloc, Bs_mem_map());

  // map kernel to desired virtual address
  map_ram_range(d, alloc, bs_info.kernel_start_phys, bs_info.kernel_end_phys,
                Virt_ofs + load_addr, Page::Kern::Global());

  // Add 1:1 mapping if not already done so above. Needed by Fiasco bootstrap
  // and the mp trampoline after they enable paging, and by add_initial_pmem().
  if (Virt_ofs + load_addr != 0)
    map_ram_range(d, alloc, bs_info.kernel_start_phys, bs_info.kernel_end_phys,
                  0, Page::Kern::Global());

  asm volatile (
      "msr tcr_el2, %1   \n"
      "dsb sy            \n"
      "msr ttbr0_el2, %0 \n"
      "isb               \n"
      : :
      "r"(d), "r"(Mword{Page::Ttbcr_bits}));

  return Phys_addr(0);
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !mmu && !cpu_virt]:

PUBLIC static void
Bootstrap::leave_hyp_mode()
{
  Mword cel;
  asm volatile ("mrs %0, CurrentEL" : "=r"(cel));
  cel >>= 2;
  cel &= 3;

  switch (cel)
    {
    case 2:
      asm volatile ("msr VTCR_EL2, %0" : : "r"(0)); // Ensure PMSAv8-64@EL1
      asm volatile ("msr S3_4_C2_C6_2, %0" : : "r"(1UL << 31)); // VSTCR_EL2
      switch_from_el2_to_el1();
      break;
    case 3: // ARMv8-R AArch64 has no EL3
    case 1:
    default:
      break;
    }
}

// -----------------------------------------------------------------
IMPLEMENTATION [arm && !mmu && cpu_virt]:

PUBLIC static inline void
Bootstrap::leave_hyp_mode()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu]:

PUBLIC static inline void
Bootstrap::init_node_data()
{}
