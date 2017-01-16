IMPLEMENTATION [arm && arm_lpae && !cpu_virt]:

#include "cpu.h"

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

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt]:

extern char kernel_l0_dir[];
extern char kernel_l1_dir[];
extern char kernel_l0_vdir[];
extern char kernel_l1_vdir[];
extern char kernel_l2_mmio_dir[];

static inline void
switch_from_el3_to_el1()
{
  // drain the data and instruction caches as we might switch from
  // secure to non-secure mode and only secure mode can clear
  // caches for secure memory.
  Mmu<Bootstrap::Cache_flush_area, true>::flush_cache();

  Mword pfr0;
  asm volatile ("mrs %0, id_aa64pfr0_el1" : "=r"(pfr0));
  if (((pfr0 >> 8) & 0xf) != 0)
    {
      // EL2 supported, set HCR (RW and HCD)
      asm volatile ("msr HCR_EL2, %0" : : "r"((1 << 29) | (1 << 31)));
    }

  // flush all E1 TLBs
  asm volatile ("tlbi alle1");

  // setup SCR (disable monitor completely)
  asm volatile ("msr scr_el3, %0"
                : :
                "r"(Cpu::Scr_ns | Cpu::Scr_rw | Cpu::Scr_smd));

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
      : [psr]"r"((0xf << 6) | 5)
      : "cc", "x4");
}

static void
Bootstrap::leave_hyp_mode()
{
  Mword cel;
  asm volatile ("mrs %0, CurrentEL" : "=r"(cel));
  cel >>= 2;
  cel &= 3;
  Mword tmp;

  switch (cel)
    {
    case 3:
      switch_from_el3_to_el1();
      break;

    case 2:
      // flush all E1 TLBs
      asm volatile ("tlbi alle1");
      // set HCR (RW and HCD)
      asm volatile ("msr HCR_EL2, %0" : : "r"((1 << 29) | (1 << 31)));
      asm volatile ("   mov %[tmp], sp       \n"
                    "   msr spsr_el2, %[psr] \n"
                    "   adr x4, 1f           \n"
                    "   msr elr_el2, x4      \n"
                    "   eret                 \n"
                    "1: mov sp, %[tmp]       \n"
                    : [tmp]"=&r"(tmp)
                    : [psr]"r"((0xf << 6) | 5)
                    : "cc", "x4");
      break;
    case 1:
    default:
      break;
    }
}

constexpr unsigned l0_idx(Unsigned64 va)
{ return (va >> 39) & 0x1ff; }

constexpr unsigned l1_idx(Unsigned64 va)
{ return (va >> 30) & 0x1ff; }

PUBLIC static Bootstrap::Phys_addr
Bootstrap::init_paging(void *)
{
  leave_hyp_mode();

  Phys_addr *const l0 = reinterpret_cast<Phys_addr*>(kernel_l0_dir + Virt_ofs);
  Phys_addr *const l1 = reinterpret_cast<Phys_addr*>(kernel_l1_dir + Virt_ofs);

  l0[l0_idx(Mem_layout::Sdram_phys_base)]
    = Phys_addr((Unsigned64)kernel_l1_dir + Virt_ofs) | Phys_addr(3);

  Phys_addr *const vl0 = reinterpret_cast<Phys_addr*>(kernel_l0_vdir + Virt_ofs);
  Phys_addr *const vl1 = reinterpret_cast<Phys_addr*>(kernel_l1_vdir + Virt_ofs);

  static_assert(l0_idx(Mem_layout::Map_base) == l0_idx(Mem_layout::Registers_map_start),
                "Mem_layout::Map_base and Mem_layout::Registers_map_start must share the 516GB index");

  vl0[l0_idx(Mem_layout::Map_base)]
    = Phys_addr((Unsigned64)kernel_l1_vdir + Virt_ofs) | Phys_addr(3);

  vl1[l1_idx(Mem_layout::Registers_map_start)]
    = Phys_addr((Unsigned64)kernel_l2_mmio_dir + Virt_ofs) | Phys_addr(3);

  set_mair0(Page::Mair0_prrr_bits);

  l1[l1_idx(Mem_layout::Sdram_phys_base)]
    = pt_entry(Phys_addr(Mem_layout::Sdram_phys_base), true, true);

  vl1[l1_idx(Mem_layout::Map_base)]
    = pt_entry(Phys_addr(Mem_layout::Sdram_phys_base), true, false);


  unsigned long attribs =   (3UL << 4)  /* SH == IS */
                          | (1UL << 2)  /* ORGN = normal outer write-back write-allocate */
                          | (1UL << 0); /* IRGN = normal inner write-back write-allocate */

  asm volatile (
      "msr tcr_el1, %2   \n"
      "dsb sy            \n"
      "msr ttbr0_el1, %0 \n"
      "msr ttbr1_el1, %1 \n"
      "isb \n"
      : :
      "r"(l0),
      "r"(vl0),
      "r"(  (5UL << 32) | (2UL << 30) | (attribs << 24) | (16UL << 16)
          |               (0UL << 14) | (attribs <<  8) | (16UL << 0)));



  return Phys_addr(0);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

static inline
Bootstrap::Order
Bootstrap::map_page_order()
{ return Order(30); }

static inline void
Bootstrap::set_mair0(Mword v)
{ asm volatile ("msr MAIR_EL1, %0" : : "r"(v)); }

PUBLIC static inline void
Bootstrap::create_initial_mappings(void *)
{}

PUBLIC static inline void
Bootstrap::add_initial_pmem()
{}

asm
(
".section .text.init,#alloc,#execinstr \n"
".global _start                        \n"
"_start:                               \n"
"     ldr x9, =_stack                  \n"
"     mov sp, x9                       \n"
"     bl	bootstrap_main         \n"
".previous                             \n"
".section .bss                         \n"
".p2align 4                            \n"
"	.space	4096                   \n"
"_stack:                               \n"
".previous                             \n"
);


