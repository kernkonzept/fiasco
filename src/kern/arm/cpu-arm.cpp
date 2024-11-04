INTERFACE [arm]:

#include "io.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "types.h"
#include "per_cpu_data.h"
#include "processor.h"

EXTENSION
class Cpu
{
public:
  void init(bool resume, bool is_boot_cpu);

  static void early_init();

  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  enum {
    Cp15_c1_mmu             = 1 << 0,
    Cp15_c1_alignment_check = 1 << 1,
    Cp15_c1_cache           = 1 << 2,
    Cp15_c1_branch_predict  = 1 << 11,
    Cp15_c1_v7_sw           = 1 << 10,
    Cp15_c1_insn_cache      = 1 << 12,
    Cp15_c1_high_vector     = 1 << 13,
  };

  Cpu(Cpu_number id) { set_id(id); }


  struct Ids {
    Mword _pfr[2], _dfr0, _afr0, _mmfr[4];
  };
  void id_init();

  enum {
    Copro_dbg_model_not_supported = 0,
    Copro_dbg_model_v6            = 2,
    Copro_dbg_model_v6_1          = 3,
    Copro_dbg_model_v7            = 4,
    Copro_dbg_model_v7_1          = 5,
    Copro_dbg_model_v8            = 6,
    Copro_dbg_model_v8_plus_vhe   = 7,
    Copro_dbg_model_v8_2          = 8,
    Copro_dbg_model_v8_4          = 9,
  };

  enum : Unsigned64
  {
    Hcr_vm     = 1UL << 0,  ///< Virtualization enable
    Hcr_swio   = 1UL << 1,  ///< Set/way invalidation override
    Hcr_ptw    = 1UL << 2,  ///< Protected table walk
    Hcr_fmo    = 1UL << 3,  ///< Physical FIQ routing
    Hcr_imo    = 1UL << 4,  ///< Physical IRQ routing
    Hcr_amo    = 1UL << 5,  ///< Physical SError interrupt routing
    Hcr_dc     = 1UL << 12, ///< Default cacheability
    Hcr_tid2   = 1UL << 17, ///< Trap CTR, CESSLR, etc.
    Hcr_tid3   = 1UL << 18, ///< Trap ID, etc.
    Hcr_tsc    = 1UL << 19, ///< Trap SMC instructions
    Hcr_tidcp  = 1UL << 20, ///< Trap implementation defined functionality
    Hcr_tactlr = 1UL << 21, ///< Trap ACTLR, etc.
    Hcr_tsw    = 1UL << 22, ///< Trap cache maintenance instructions
    Hcr_ttlb   = 1UL << 25, ///< Trap TLB maintenance instructions
    Hcr_tvm    = 1UL << 26, ///< Trap virtual memory controls
    Hcr_tge    = 1UL << 27, ///< Trap General Exceptions
    Hcr_hcd    = 1UL << 29, ///< HVC instruction disable
    Hcr_trvm   = 1UL << 30, ///< Trap reads of virtual memory controls
    Hcr_rw     = 1UL << 31, ///< EL1 is AArch64
    Hcr_tlor   = 1ULL << 35, ///< Trap FEAT_LOR registers, not defined for HCR2
  };

  enum : bool
  {
    Has_el3 = TAG_ENABLED(arm_profile_a), // Only the A-profile sports EL3
  };

  unsigned copro_dbg_model() const { return _cpu_id._dfr0 & 0xf; }

  bool has_pmuv1() const;
  bool has_pmuv2() const;
  bool has_pmuv3() const;

private:
  void init_supervisor_mode(bool is_boot_cpu);
  void init_hyp_mode();
  static void early_init_platform();

  static Global_data<Cpu *> _boot_cpu;

  // 32 bits: 24..31: Aff3 (0 for ARM32); 16..23: Aff2; 8..15: Aff1; 0..7: Aff0
  Cpu_phys_id _phys_id;
  Ids _cpu_id;

  bool has_hpmn0() const;
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_v5]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_write_buffer    = 1 << 3,
    Cp15_c1_prog32          = 1 << 4,
    Cp15_c1_data32          = 1 << 5,
    Cp15_c1_late_abort      = 1 << 6,
    Cp15_c1_big_endian      = 1 << 7,
    Cp15_c1_system_protect  = 1 << 8,
    Cp15_c1_rom_protect     = 1 << 9,
    Cp15_c1_f               = 1 << 10,
    Cp15_c1_rr              = 1 << 14,
    Cp15_c1_l4              = 1 << 15,

    Cp15_c1_generic         = Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  Cp15_c1_alignment_check : 0)
                              | Cp15_c1_write_buffer
                              | Cp15_c1_prog32
                              | Cp15_c1_data32
                              | Cp15_c1_late_abort
                              | Cp15_c1_rom_protect
                              | Cp15_c1_high_vector,

    Cp15_c1_cache_bits      = Cp15_c1_cache
                              | Cp15_c1_insn_cache
                              | Cp15_c1_write_buffer,

  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_v6]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_l4              = 1 << 15,
    Cp15_c1_u               = 1 << 22,
    Cp15_c1_xp              = 1 << 23,
    Cp15_c1_ee              = 1 << 25,
    Cp15_c1_nmfi            = 1 << 27,
    Cp15_c1_tre             = 1 << 28,
    Cp15_c1_force_ap        = 1 << 29,

    Cp15_c1_cache_bits      = Cp15_c1_cache
                              | Cp15_c1_insn_cache,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_v6 && !arm_mpcore]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_generic         = Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  Cp15_c1_alignment_check : 0)
			      | Cp15_c1_branch_predict
			      | Cp15_c1_high_vector
                              | Cp15_c1_u
			      | Cp15_c1_xp,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && arm_v6 && arm_mpcore]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_generic         = Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check
                                 ? Cp15_c1_alignment_check : 0)
                              | Cp15_c1_branch_predict
                              | Cp15_c1_high_vector
                              | Cp15_c1_u
                              | Cp15_c1_xp
                              | Cp15_c1_tre,
  };
};


// ------------------------------------------------------------------------
INTERFACE [arm && (arm_v7 || arm_v8) && mmu]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_ha              = 1 << 17,
    Cp15_c1_ee              = 1 << 25,
    Cp15_c1_nmfi            = 1 << 27,
    Cp15_c1_tre             = 1 << 28,
    Cp15_c1_te              = 1 << 30,
    Cp15_c1_rao_sbop        = (0xf << 3) | (1 << 16) | (1 << 18) | (1 << 22) | (1 << 23),

    Cp15_c1_cache_bits      = Cp15_c1_cache
                              | Cp15_c1_insn_cache,

    Cp15_c1_generic         = Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  Cp15_c1_alignment_check : 0)
                              | Cp15_c1_branch_predict
                              | Cp15_c1_high_vector
                              | Cp15_c1_tre
                              | Cp15_c1_rao_sbop,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && (arm_v7 || arm_v8) && mpu]:

EXTENSION class Cpu
{
public:
  enum {
    Cp15_c1_nmfi            = 1 << 27,
    Cp15_c1_rao_sbop        = (0xf << 3) | (1 << 16) | (1 << 18) | (1 << 22) | (1 << 23),

    Cp15_c1_cache_bits      = Cp15_c1_cache
                              | Cp15_c1_insn_cache,

    Cp15_c1_generic         = Cp15_c1_mmu
                              | (Config::Cp15_c1_use_alignment_check ?  Cp15_c1_alignment_check : 0)
                              | Cp15_c1_branch_predict
                              | Cp15_c1_rao_sbop,
  };
};

//--------------------------------------------------------
INTERFACE [arm && cpu_virt]:

EXTENSION class Cpu
{
public:
  enum
  {
    // HDCR[31:0] (arm32) is architecturally mapped to MDCR_EL2[31:0] (arm64).
    Mdcr_hpmn_mask = 0xf,
    Mdcr_tpmcr     = 1UL << 5,
    Mdcr_tpm       = 1UL << 6,
    Mdcr_hpme      = 1UL << 7,
    Mdcr_tde       = 1UL << 8,
    Mdcr_tda       = 1UL << 9,
    Mdcr_tdosa     = 1UL << 10,
    Mdcr_tdra      = 1UL << 11,
    Mdcr_tpms      = 1UL << 14,
    Mdcr_ttrf      = 1UL << 19,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && (arm_v7 || arm_v8) && cpu_virt]:

EXTENSION class Cpu
{
public:
  enum Hstr_values
  {
    Hstr_non_vm = (1 << 0)
                | (1 << 1)
                | (1 << 2)
                | (1 << 3)
                | (0 << 4) // res0
                | (1 << 5)
                | (1 << 6)
                | (0 << 7) // enable data and instruction barrier operations
                | (1 << 8)
                | ((TAG_ENABLED(perf_cnt_user) ? 0 : 1) << 9) // PMCCNTR
                | (1 << 10)
                | (1 << 11)
                | (1 << 12)
                | (0 << 13) // enable access to TPIDRxxR
                | (0 << 14) // res0
                | (1 << 15)
                | ((TAG_ENABLED(arm_v7) ? 1 : 0) << 16) // TTEE, only ARMv7
                | ((TAG_ENABLED(arm_v7) ? 1 : 0) << 17) // TJDBX, only ARMv7
                ,
    Hstr_vm = 0x0, // none
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && (arm_v7 || arm_v8) && mpu && cpu_virt]:

EXTENSION class Cpu
{
public:
  enum {
    Hsctlr_res1 = (3 << 28) | (3 << 22) | (1 << 18) | (1 << 16) | (1 << 11)
                | (3 << 3),

    Hsctlr_cache_bits = (1 << 12) | (1 << 2),
    Hsctlr_alignment_check = 1 << 1,
    Hsctlr_cp15ben = 1 << 5,
    Hsctlr_mpu = 1 << 0,
    Hsctlr_fi = 1 << 21,

    Hsctlr = (Config::Cp15_c1_use_alignment_check ?  Hsctlr_alignment_check : 0)
           | (Config::Cache_enabled ? Hsctlr_cache_bits : 0)
           | (Config::Fast_interrupts ? Hsctlr_fi : 0)
           | Hsctlr_cp15ben
           | Hsctlr_mpu
           | Hsctlr_res1,
  };
};

// -------------------------------------------------------------------------------
INTERFACE [arm]:

EXTENSION class Cpu
{
private:
  static void bsp_init(bool);
};

//-------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT_DEFAULT static inline
void
Cpu::bsp_init(bool) {}

IMPLEMENT_DEFAULT inline
bool
Cpu::has_hpmn0() const
{
  return false;
}

IMPLEMENT_DEFAULT inline
bool
Cpu::has_pmuv1() const
{
  return false;
}

IMPLEMENT_DEFAULT inline
bool
Cpu::has_pmuv2() const
{
  return false;
}

IMPLEMENT_DEFAULT inline
bool
Cpu::has_pmuv3() const
{
  return false;
}

IMPLEMENTATION [arm && arm_v6]: // -----------------------------------------

PUBLIC static inline NEEDS[Cpu::set_actrl]
void
Cpu::enable_smp()
{
  set_actrl(0x20);
}

PUBLIC static inline NEEDS[Cpu::clear_actrl]
void
Cpu::disable_smp()
{
  clear_actrl(0x20);
}

IMPLEMENTATION [arm && (arm_v7 || arm_v8) && 32bit]: //----------------------

static void modify_actl(Unsigned64 mask, Unsigned64 value)
{
  Mword actrl;
  asm volatile ("mrc p15, 0, %0, c1, c0, 1" : "=r" (actrl));
  if ((actrl & mask) != value)
    asm volatile ("mcr p15, 0, %0, c1, c0, 1" : : "r" ((actrl & mask) | value));
}

static void modify_cpuectl(Unsigned64 mask, Unsigned64 value)
{
  Mword ectlh, ectll;
  asm volatile ("mrrc p15, 1, %0, %1, c15" : "=r"(ectll), "=r"(ectlh));
  Unsigned64 ectl = (Unsigned64{ectlh} << 32) | ectll;
  if ((ectl & mask) != value)
    asm volatile ("mcrr p15, 1, %0, %1, c15" : :
                  "r"((ectll & mask) | value),
                  "r"((ectlh & (mask >> 32)) | (value >> 32)));
}

struct Midr_match
{
  Unsigned32 mask;
  Unsigned32 value;
  Unsigned64 f_mask;
  Unsigned64 f_value;
  void (*func)(Unsigned64 mask, Unsigned64 value);
};

static Midr_match constexpr _enable_smp[] =
{
  { 0xff0ffff0, 0x410fc050, 0x41, 0x41, &modify_actl },   // Cortex-A5
  { 0xff0ffff0, 0x410fc070, 0x40, 0x40, &modify_actl },   // Cortex-A7
  { 0xff0ffff0, 0x410fc090, 0x41, 0x41, &modify_actl },   // Cortex-A9
  { 0xff0ffff0, 0x410fc0d0, 0x41, 0x41, &modify_actl },   // Cortex-A12
  { 0xff0ffff0, 0x410fc0e0, 0x41, 0x41, &modify_actl },   // Cortex-A17
  { 0xff0ffff0, 0x410fc0f0, 0x41, 0x41, &modify_actl },   // Cortex-A15
  { 0xff0ffff0, 0x410fd040, 0x40, 0x40, &modify_cpuectl }, // Cortex-A35
  { 0xff0ffff0, 0x410fd030, 0x40, 0x40, &modify_cpuectl }, // Cortex-A53
  { 0xff0ffff0, 0x410fd070, 0x40, 0x40, &modify_cpuectl }, // Cortex-A57
  { 0xff0ffff0, 0x410fd080, 0x40, 0x40, &modify_cpuectl }, // Cortex-A72
};

PUBLIC static
void
Cpu::enable_smp()
{
  Unsigned32 m = midr();
  for (auto const &e : _enable_smp)
    if ((e.mask & m) == e.value)
      {
        e.func(e.f_mask, e.f_value);
        break;
      }
}

PUBLIC static
void
Cpu::disable_smp()
{
  Unsigned32 m = midr();
  for (auto const &e : _enable_smp)
    if ((e.mask & m) == e.value)
      {
        e.func(e.f_mask, ~e.f_value & e.f_mask);
        break;
      }
}

//---------------------------------------------------------------------------
INTERFACE [arm && (arm_mpcore || arm_cortex_a9)]:

#include "scu.h"

EXTENSION class Cpu
{
public:
 static Static_object<Scu> scu;
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_mpcore || arm_cortex_a9)]:

#include "kmem_mmio.h"

Static_object<Scu> Cpu::scu;

PRIVATE static
void
Cpu::init_scu()
{
  scu.construct(Kmem_mmio::map(Mem_layout::Mp_scu_phys_base,
                               Config::PAGE_SIZE));

  scu->reset();
  scu->enable(Scu::Bsp_enable_bits);
}

IMPLEMENT_OVERRIDE inline NEEDS[Cpu::init_scu]
void
Cpu::early_init_platform()
{
  init_scu();
  Mem_unit::clean_dcache();
  enable_smp();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [(arm_v7 || arm_v8) && !arm_cortex_a9]:

#include "kmem.h"

IMPLEMENT_OVERRIDE inline NEEDS["kmem.h"]
void
Cpu::early_init_platform()
{
  Mem_unit::clean_dcache();
  enable_smp();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include <cstdio>
#include <cstring>
#include <panic.h>

#include "io.h"
#include "paging.h"
#include "kmem_space.h"
#include "kmem_alloc.h"
#include "mem_unit.h"
#include "processor.h"
#include "ram_quota.h"

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
DEFINE_GLOBAL Global_data<Cpu *> Cpu::_boot_cpu;

IMPLEMENT_DEFAULT inline void Cpu::early_init_platform() {}

PUBLIC static inline
Mword
Cpu::stack_align(Mword stack)
{ return stack & ~0x3; }



PUBLIC static inline
bool
Cpu::have_superpages()
{ return true; }

PUBLIC static inline
void
Cpu::debugctl_enable()
{}

PUBLIC static inline
void
Cpu::debugctl_disable()
{}

PUBLIC static inline NEEDS["types.h"]
Unsigned32
Cpu::get_scaler_tsc_to_ns()
{ return 0; }

PUBLIC static inline NEEDS["types.h"]
Unsigned32
Cpu::get_scaler_tsc_to_us()
{ return 0; }

PUBLIC static inline NEEDS["types.h"]
Unsigned32
Cpu::get_scaler_ns_to_tsc()
{ return 0; }

PUBLIC static inline
bool
Cpu::tsc()
{ return 0; }

PUBLIC static inline
Unsigned64
Cpu::rdtsc (void)
{ return 0; }

PUBLIC inline
Cpu_phys_id
Cpu::phys_id() const
{ return _phys_id; }

IMPLEMENT
void
Cpu::init(bool /*resume*/, bool is_boot_cpu)
{
  if (is_boot_cpu)
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
    }

  _phys_id = Proc::cpu_id();

  init_tz();
  id_init();
  init_errata_workarounds();
  init_supervisor_mode(is_boot_cpu);
  init_hyp_mode();
  bsp_init(is_boot_cpu);
}

IMPLEMENT_DEFAULT inline
void
Cpu::init_supervisor_mode(bool)
{}

IMPLEMENT_DEFAULT inline
void
Cpu::init_hyp_mode()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_v6plus]:

IMPLEMENT
void
Cpu::id_init()
{
}

//---------------------------------------------------------------------------
IMPLEMENTATION [!arm_cpu_errata || !arm_v6plus || 64bit]:

PRIVATE static inline
void Cpu::init_errata_workarounds() {}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm_cpu_errata && arm_v6plus && 32bit]:

PRIVATE static inline
void
Cpu::set_c15_c0_1(Mword bits_mask)
{
  Mword t;
  asm volatile("mrc p15, 0, %0, c15, c0, 1 \n\t"
               "orr %0, %0, %1             \n\t"
               "mcr p15, 0, %0, c15, c0, 1 \n\t"
               : "=r"(t) : "r" (bits_mask));
}

PRIVATE static inline NEEDS[Cpu::midr]
void
Cpu::init_errata_workarounds()
{
  Mword mid = midr();

  if ((mid & 0xff000000) == 0x41000000) // ARM CPU
    {
      Mword rev = ((mid & 0x00f00000) >> 16) | (mid & 0x0f);
      Mword part = (mid & 0x0000fff0) >> 4;

      if (part == 0xc08) // Cortex A8
        {
          // errata: 430973
          if ((rev & 0xf0) == 0x10)
            set_actrl(1 << 6); // IBE to 1

          // errata: 458693
          if (rev == 0x20)
            set_actrl((1 << 5) | (1 << 9)); // L1NEON & PLDNOP

          // errata: 460075
          if (rev == 0x20)
            {
              Mword t;
              asm volatile ("mrc p15, 1, %0, c9, c0, 2 \n\t"
                            "orr %0, %0, #1 << 22      \n\t" // Write alloc disable
                            "mcr p15, 1, %0, c9, c0, 2 \n\t" : "=r"(t));
            }
        }

      if (part == 0xc09) // Cortex A9
        {
          // errata: 742230 (DMB errata)
          // make DMB a DSB to fix behavior
          if (rev <= 0x22) // <= r2p2
            set_c15_c0_1(1 << 4);

          // errata: 742231
          if (rev == 0x20 || rev == 0x21 || rev == 0x22)
            set_c15_c0_1((1 << 12) | (1 << 22));

          // errata: 743622
          if ((rev & 0xf0) == 0x20)
            set_c15_c0_1(1 << 6);

          // errata: 751472
          if (rev < 0x30)
            set_c15_c0_1(1 << 11);
        }

      if (part == 0xd13)  // Cortex R52
        {
          if (Config::Fast_interrupts)
            {
              // Erratum 2918152 (LDM data corruption if HSCTLR.FI set)
              Unsigned64 v;
              asm("mrrc p15, 0, %Q0, %R0, c15" : "=r"(v)); // CPUACTLR
              v |= Unsigned64{1} << 45; // OOODIVDIS
              asm("mcrr p15, 0, %Q0, %R0, c15" : : "r"(v)); // CPUACTLR
            }
        }
    }
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_em_tz]:

PRIVATE static inline
void
Cpu::init_tz()
{}

//---------------------------------------------------------------------------
INTERFACE [arm && arm_em_tz]:

EXTENSION class Cpu
{
public:
  static char monitor_vector_base asm ("monitor_vector_base");
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_em_tz]:

#include <cassert>

PRIVATE inline NEEDS[<cassert>]
void
Cpu::init_tz()
{
  Mword sctrl;
  asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r" (sctrl));
  if (sctrl & Cp15_c1_nmfi)
    panic("Non-maskable FIQs (NMFI) detected, cannot use TZ mode");

  // set monitor vector base address
  assert(!(reinterpret_cast<Mword>(&monitor_vector_base) & 31));
  asm volatile ("mcr p15, 0, %0, c12, c0, 1" : : "r" (&monitor_vector_base));

  Mword dummy;
  asm volatile (
      "mov  %[dummy], sp \n"
      "cps  #0x16        \n"
      "mov  sp, %[dummy] \n"
      : [dummy] "=r" (dummy) : : "lr" );
  // running in monitor mode

  asm ("mcr  p15, 0, %[scr], c1, c1, 0" : : [scr] "r" (0x1));
  Mem::isb();

  asm ("mcr  p15, 0, %0, c12, c0, 0" : : "r" (0)); // reset VBAR
  asm ("mcr  p15, 0, %0, c13, c0, 0" : : "r" (0)); // reset FCSEIDR
  asm ("mcr  p15, 0, %0, c1, c0, 0"  : : "r" (0x4500a0));  // SCTLR = U | (18) | (16) | (7)

  asm ("mcr  p15, 0, %[scr], c1, c1, 0 \n" : : [scr] "r" (0x100));
  Mem::isb();
  asm volatile (
      "mov  %[dummy], sp \n"
      "cps  #0x13        \n"
      "mov  sp, %[dummy] \n"
      : [dummy] "=r" (dummy) : : "lr" );
  // running in svc mode


  // enable nonsecure access to vfp coprocessor
  asm volatile("mcr p15, 0, %0, c1, c1, 2" : : "r" (0xc00));

  enum
  {
    SCR_NS  = 1 << 0,
    SCR_IRQ = 1 << 1,
    SCR_FIQ = 1 << 2,
    SCR_EA  = 1 << 3,
    SCR_FW  = 1 << 4,
    SCR_AW  = 1 << 5,
    SCR_nET = 1 << 6,
    SCR_SCD = 1 << 7,
    SCR_HCE = 1 << 8,
    SCR_SIF = 1 << 9,
  };
}

PUBLIC inline
void
Cpu::tz_switch_to_ns(Mword *nonsecure_state)
{
  extern char go_nonsecure[];

  register Mword r0 asm("r0") = reinterpret_cast<Mword>(nonsecure_state);
  register Mword r1 asm("r1") = reinterpret_cast<Mword>(go_nonsecure);

  asm volatile(
#ifdef __thumb__
               "push   {r7}       \n"
#else
               "push   {r11}      \n"
#endif
               "stmdb sp!, {r0}   \n"
               "mov    r2, sp     \n" // copy sp_svc to sp_mon
               "cps    #0x16      \n" // switch to monitor mode
               "mov    sp, r2     \n"
               "mrs    r4, cpsr   \n" // save return psr
#ifdef __thumb__
               "adr    r3, (1f + 1)\n" // save return eip
#else
               "adr    r3, 1f     \n" // save return eip
#endif
               "bx     r1         \n" // go nonsecure!
               "1:                \n"
               "mov    r0, sp     \n" // copy sp_mon to sp_svc
               "cps    #0x13      \n" // switch to svc mode
               "mov    sp, r0     \n"
               "ldmia  sp!, {r0}  \n"
#ifdef __thumb__
               "pop    {r7}       \n"
#else
               "pop    {r11}      \n"
#endif
               : : "r" (r0), "r" (r1)
               : "r2", "r3", "r4", "r5", "r6", "r8", "r9", "r10", "r12", "r14",
#ifdef __thumb__
                 "r11",
#else
                 "r7",
#endif
                 "memory");
}

PUBLIC static inline
Mword
Cpu::tz_scr()
{
  Mword r;
  asm volatile ("mrc p15, 0, %0, c1, c1, 0" : "=r" (r));
  return r;
}

PUBLIC static inline
void
Cpu::tz_scr(Mword val)
{
  asm volatile ("mcr p15, 0, %0, c1, c1, 0" : : "r" (val));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [bit64]:

IMPLEMENT_OVERRIDE inline
bool
Cpu::is_canonical_address(Address addr)
{
  // cf. ARMv8-A Address Translation
  return addr >= 0xffff000000000000UL || addr <= 0x0000ffffffffffffUL;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

#include "feature.h"

KIP_KERNEL_FEATURE("arm:hyp");

// ------------------------------------------------------------------------
IMPLEMENTATION [!debug]:

PUBLIC inline
void
Cpu::print_infos() const
{}

PUBLIC static inline
void
Cpu::print_boot_infos()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug && arm_v6plus]:

PUBLIC
void
Cpu::print_infos() const
{
  int n = cxx::int_value<Cpu_number>(current_cpu());
  printf("CPU%u: ID_PFR[01]:  %08lx %08lx ID_[DA]FR0: %08lx %08lx\n"
         "%*s ID_MMFR[04]: %08lx %08lx %08lx %08lx\n",
         n, _cpu_id._pfr[0], _cpu_id._pfr[1], _cpu_id._dfr0, _cpu_id._afr0,
         n > 9 ? 6 : 5, "",
         _cpu_id._mmfr[0], _cpu_id._mmfr[1], _cpu_id._mmfr[2], _cpu_id._mmfr[3]);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug && !arm_v6plus]:

PRIVATE
void
Cpu::print_infos() const
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC static
void
Cpu::print_boot_infos()
{
  printf("Cache config: %s\n", Config::Cache_enabled ? "ON" : "OFF");
}
