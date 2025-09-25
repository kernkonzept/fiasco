INTERFACE [arm]:

#include "global_data.h"

EXTENSION class Cpu
{
public:
  static Global_data<Unsigned32> sctlr;
  bool has_generic_timer() const { return (_cpu_id._pfr[1] & 0xf0000) == 0x10000; }
};

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

DEFINE_GLOBAL Global_data<Unsigned32> Cpu::sctlr;

PUBLIC static inline
Mword
Cpu::midr()
{
  Mword m;
  asm volatile ("mrc p15, 0, %0, c0, c0, 0" : "=r" (m));
  return m;
}

PUBLIC static inline
Mword
Cpu::mpidr()
{
  Mword mpid;
  asm volatile ("mrc p15, 0, %0, c0, c0, 5" : "=r"(mpid));
  return mpid;
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v7plus]:

IMPLEMENT_OVERRIDE inline
bool
Cpu::has_pmuv1() const
{
  unsigned pmuv = (_cpu_id._dfr0 >> 24) & 0xf;
  enum { Cortex_a9 = 0x410FC090 };
  // ID_DFR0[24:27] = 0 is no indication if PMUv1 is supported or not.
  // ARM Cortex-A9 has PMUv1 but ID_DFR0 is 0x00010444.
  return (pmuv >= 1 && pmuv != 0xf) || (midr() & 0xff0ffff0UL) == Cortex_a9;
}

IMPLEMENT_OVERRIDE inline
bool
Cpu::has_pmuv2() const
{
  unsigned pmuv = (_cpu_id._dfr0 >> 24) & 0xf;
  return pmuv >= 2 && pmuv != 0xf;
}

IMPLEMENT_OVERRIDE inline
bool
Cpu::has_pmuv3() const
{
  unsigned pmuv = (_cpu_id._dfr0 >> 24) & 0xf;
  return pmuv >= 3 && pmuv != 0xf;
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8plus && mmu]:

PUBLIC static inline
Mword
Cpu::dfr1()
{ Mword r; asm volatile ("mrc p15, 0, %0, c0, c3, 5": "=r" (r)); return r; }

IMPLEMENT_OVERRIDE inline
bool
Cpu::has_hpmn0() const
{ return ((dfr1() >> 4) & 0xf) == 1; }

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cxx/conditionals>

PRIVATE static inline
void
Cpu::check_for_swp_enable()
{
  if constexpr (!Config::Sctlr_use_swp_enable)
    return;

  if (((midr() >> 16) & 0xf) != 0xf)
    return; // pre ARMv7 has no swap enable / disable

  Mword id_isar0;
  asm volatile ("mrc p15, 0, %0, c0, c2, 0" : "=r"(id_isar0));
  if ((id_isar0 & 0xf) != 1)
    return; // CPU has no swp / swpb

  if (((mpidr() >> 31) & 1) == 0)
    return; // CPU has no MP extensions -> no swp enable

  sctlr |= Sctlr_v7_sw;
}

IMPLEMENT
void Cpu::early_init()
{
  sctlr = Cpu::Sctlr_generic;

  check_for_swp_enable();

  // switch to supervisor mode and initialize the memory system
  asm volatile ( " mov  r2, r13             \n"
                 " mov  r3, r14             \n"
                 " msr  cpsr_c, %1          \n"
                 " mov  r13, r2             \n"
                 " mov  r14, r3             \n"

                 " mcr  p15, 0, %0, c1, c0  \n"
                 :
                 : "r" (sctlr.unwrap()),
                   "r" (Proc::Status_mode_supervisor
                        | Proc::Status_interrupts_disabled)
                 : "r2", "r3");

  early_init_platform();

  Mem_unit::flush_cache();
}

PUBLIC static inline
void
Cpu::enable_dcache()
{
  asm volatile("mrc     p15, 0, %0, c1, c0, 0 \n"
               "orr     %0, %1                \n"
               "mcr     p15, 0, %0, c1, c0, 0 \n"
               : : "r" (0), "i" (Sctlr_cache));
}

PUBLIC static inline
void
Cpu::disable_dcache()
{
  asm volatile("mrc     p15, 0, %0, c1, c0, 0 \n"
               "bic     %0, %1                \n"
               "mcr     p15, 0, %0, c1, c0, 0 \n"
               : : "r" (0), "i" (Sctlr_cache));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt && mmu]:

#include "kmem.h"
#include "kmem_space.h"

IMPLEMENT_OVERRIDE
void
Cpu::init_supervisor_mode(bool is_boot_cpu)
{
  if (!is_boot_cpu)
    return;

  extern char ivt_start; // physical address!

  // map the interrupt vector table to 0xffff0000
  auto pte = Kmem::kdir->walk(Virt_addr(Kmem_space::Ivt_base),
                              Kpdir::Depth, true,
                              Kmem_alloc::q_allocator(Ram_quota::root.unwrap()));

  Address va = reinterpret_cast<Address>(&ivt_start)
                 - Mem_layout::Sdram_phys_base + Mem_layout::Map_base;
  pte.set_page(Phys_mem_addr(Kmem::kdir->virt_to_phys(va)),
               Page::Attr::kern_global(Page::Rights::RWX()));
  pte.write_back_if(true);
  Mem_unit::tlb_flush_kernel(Kmem_space::Ivt_base);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !cpu_virt && mpu]:

PUBLIC static
void
Cpu::init_sctlr()
{
  unsigned control = Sctlr_generic;

  Mem::dsb();
  asm volatile("mcr p15, 0, %[control], c1, c0, 0" // SCTLR
      : : [control] "r" (control));
  Mem::isb();
}

IMPLEMENT_OVERRIDE
void
Cpu::init_supervisor_mode(bool)
{
  // set VBAR system register to exception vector address
  extern char exception_vector;
  asm volatile("mcr p15, 0, %0, c12, c0, 0 \n\t"  // VBAR
               :  : "r" (&exception_vector));

  // make sure vectors are executed in A32 state
  unsigned long r;
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (r) : : "memory");  // SCTLR
  r &= ~(1UL << 30);
  asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r" (r) : "memory");   // SCTLR
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && mpu]:

PUBLIC static
void
Cpu::init_sctlr()
{
  Mem::dsb();
  asm volatile("mcr p15, 4, %[control], c1, c0, 0" // HSCTLR
      : : [control] "r" (Hsctlr));
  Mem::isb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

PRIVATE static inline
void
Cpu::modify_actrl(Mword set_mask, Mword clear_mask)
{
  Mword t;
  asm volatile("mrc p15, 0, %[reg], c1, c0, 1 \n\t"
               "bic %[reg], %[reg], %[clr]    \n\t"
               "orr %[reg], %[reg], %[set]    \n\t"
               "mcr p15, 0, %[reg], c1, c0, 1 \n\t"
               : [reg] "=r" (t)
               : [set] "r" (set_mask), [clr] "r" (clear_mask));
}

PRIVATE static inline NEEDS[Cpu::modify_actrl]
void
Cpu::set_actrl(Mword bit_mask)
{ modify_actrl(bit_mask, 0); }

PRIVATE static inline NEEDS[Cpu::modify_actrl]
void
Cpu::clear_actrl(Mword bit_mask)
{ modify_actrl(0, bit_mask); }

IMPLEMENT
void
Cpu::id_init()
{
  __asm__("mrc p15, 0, %0, c0, c1, 0": "=r" (_cpu_id._pfr[0]));
  __asm__("mrc p15, 0, %0, c0, c1, 1": "=r" (_cpu_id._pfr[1]));
  __asm__("mrc p15, 0, %0, c0, c1, 2": "=r" (_cpu_id._dfr0));
  __asm__("mrc p15, 0, %0, c0, c1, 3": "=r" (_cpu_id._afr0));
  __asm__("mrc p15, 0, %0, c0, c1, 4": "=r" (_cpu_id._mmfr[0]));
  __asm__("mrc p15, 0, %0, c0, c1, 5": "=r" (_cpu_id._mmfr[1]));
  __asm__("mrc p15, 0, %0, c0, c1, 6": "=r" (_cpu_id._mmfr[2]));
  __asm__("mrc p15, 0, %0, c0, c1, 7": "=r" (_cpu_id._mmfr[3]));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v7]:

PUBLIC static inline
Unsigned32
Cpu::hcr()
{
  Unsigned32 r;
  asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(r));
  return r;
}

PUBLIC static inline
void
Cpu::hcr(Unsigned32 hcr)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 0" : : "r"(hcr));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

PUBLIC static inline
Unsigned64
Cpu::hcr()
{
  Unsigned32 l, h;
  asm volatile ("mrc p15, 4, %0, c1, c1, 0" : "=r"(l));
  asm volatile ("mrc p15, 4, %0, c1, c1, 4" : "=r"(h));
  return Unsigned64{h} << 32 | l;
}

PUBLIC static inline
void
Cpu::hcr(Unsigned64 hcr)
{
  asm volatile ("mcr p15, 4, %0, c1, c1, 0" : : "r"(hcr & 0xffffffff));
  asm volatile ("mcr p15, 4, %0, c1, c1, 4" : : "r"(hcr >> 32));
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !(mmu && arm_lpae)]:

PUBLIC static inline unsigned Cpu::phys_bits() { return 32; }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && mmu && arm_lpae]:

PUBLIC static inline unsigned Cpu::phys_bits() { return 40; }

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit && mmu]:

EXTENSION class Cpu
{
public:
  static constexpr Unsigned32 Hcr_must_set_bits = Hcr_vm | Hcr_swio | Hcr_ptw
                                                | Hcr_amo | Hcr_imo | Hcr_fmo
                                                | Hcr_tidcp | Hcr_tsc | Hcr_tactlr;
};

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  asm volatile (
        "mcr p15, 4, %0, c2, c1, 2" // VTCR
        : : "r" ((1UL << 31) | (Page::Tcr_attribs << 8) | (1 << 6)));
  init_hyp_mode_common();
}

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit && !mmu]:

EXTENSION class Cpu
{
public:
  static constexpr Unsigned32 Hcr_must_set_bits = Hcr_vm | Hcr_swio
                                                | Hcr_amo| Hcr_imo | Hcr_fmo
                                                | Hcr_tidcp | Hcr_tsc | Hcr_tactlr;
};

IMPLEMENT_OVERRIDE
void
Cpu::init_hyp_mode()
{
  init_hyp_mode_common();
}

//--------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt && 32bit]:

EXTENSION class Cpu
{
public:
  enum : Unsigned32
  {
    /**
     * HCR value to be used for the VMM.
     *
     * The VMM runs in system mode (PL1), but has extended
     * CP15 access allowed.
     */
    Hcr_host_bits = Hcr_must_set_bits | Hcr_dc,

    /**
     * HCR value to be used for normal threads.
     *
     * On a hyp kernel they can choose to run in PL1 or PL0.
     * However, all but the TPIDxyz CP15 accesses are disabled.
     */
    Hcr_non_vm_bits_common = Hcr_must_set_bits | Hcr_dc | Hcr_tsw
                             | Hcr_ttlb | Hcr_tvm,
    Hcr_non_vm_bits_el1    = Hcr_non_vm_bits_common,
    Hcr_non_vm_bits_el0    = Hcr_non_vm_bits_common | Hcr_tge,
  };

  enum
  {
    Hdcr_bits = (TAG_ENABLED(perf_cnt_user) ? 0 : (Mdcr_tpmcr | Mdcr_tpm))
                | Mdcr_tde | Mdcr_tda | Mdcr_tdosa | Mdcr_tdra | Mdcr_ttrf
  };
};

PRIVATE inline
void
Cpu::init_hyp_mode_common()
{
  extern char hyp_vector_base[];

  assert (!(reinterpret_cast<Mword>(hyp_vector_base) & 31));
  asm volatile ("mcr p15, 4, %0, c12, c0, 0 \n" : : "r"(hyp_vector_base));

  Mword sctlr_ignore;
  Mword hdcr;
  if (has_pmuv3())
    {
      Mword pmcr;
      asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (pmcr)); // read PMCR
      hdcr = (pmcr >> 11) & 0x1f;
    }
  else
    {
      asm volatile ("mrc p15, 4, %0, c1, c1, 1" : "=r"(hdcr)); // read HDCR
      hdcr &= 0x1f; // keep HPMN at reset value
    }
  hdcr |= Hdcr_bits;
  asm volatile (
        "mcr p15, 4, %[hdcr], c1, c1, 1 \n"     // write HDCR
        "mrc p15, 0, %[sctlr], c1, c0, 0 \n"    // read SCTLR
        "bic %[sctlr], #1 \n"                   // disable PL1&0 stage 1 MMU
        "mcr p15, 0, %[sctlr], c1, c0, 0 \n"    // write SCTLR
        : [sctlr]"=&r"(sctlr_ignore)
        : [hdcr]"r"(hdcr));
  hcr(Hcr_non_vm_bits_el0);
  asm volatile ("mcr p15, 4, %0, c1, c1, 3" : : "r"(Hstr_non_vm)); // HSTR

  Mem::dsb();
  Mem::isb();

  // HCPTR
  asm volatile("mcr p15, 4, %0, c1, c1, 2" : :
               "r" (  0x33ff       // TCP: 0-9, 12-13
                    | (1 << 20))); // TTA
}
