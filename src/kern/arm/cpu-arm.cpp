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
  static void init_mmu(bool is_boot_cpu);

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
  };

  unsigned copro_dbg_model() const { return _cpu_id._dfr0 & 0xf; }

private:
  void init_hyp_mode();
  static void early_init_platform();

  static Cpu *_boot_cpu;

  Cpu_phys_id _phys_id;
  Ids _cpu_id;
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
INTERFACE [arm && (arm_v7 || arm_v8)]:

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
  Unsigned64 ectl = (((Unsigned64)ectlh) << 32) | ectll;
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

static Midr_match _enable_smp[] =
{
  { 0xff0ffff0, 0x410fc050, 0x41, 0x41, &modify_actl },   // Cortex-A5
  { 0xff0ffff0, 0x410fc070, 0x40, 0x40, &modify_actl },   // Cortex-A7
  { 0xff0ffff0, 0x410fc090, 0x41, 0x41, &modify_actl },   // Cortex-A9
  { 0xff0ffff0, 0x410fc0d0, 0x41, 0x41, &modify_actl },   // Cortex-A12
  { 0xff0ffff0, 0x410fc0e0, 0x41, 0x41, &modify_actl },   // Cortex-A17
  { 0xff0ffff0, 0x410fc0f0, 0x41, 0x41, &modify_actl },   // Cortex-A15
  { 0xff0fff00, 0x410fd000, 0x40, 0x40, &modify_cpuectl } // Cortex-A3x/A5x/A7x
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

#include "kmem.h"

Static_object<Scu> Cpu::scu;

PRIVATE static
void
Cpu::init_scu()
{
  scu.construct(Kmem::mmio_remap(Mem_layout::Mp_scu_phys_base));

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
Cpu *Cpu::_boot_cpu;

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

IMPLEMENT_DEFAULT
void
Cpu::init_mmu(bool is_boot_cpu)
{
  if (!is_boot_cpu)
    return;

  extern char ivt_start;
  // map the interrupt vector table to 0xffff0000
  auto pte = Mem_layout::kdir->walk(Virt_addr(Kmem_space::Ivt_base),
                                    Kpdir::Depth, true,
                                    Kmem_alloc::q_allocator(Ram_quota::root));

  pte.set_page(pte.make_page(Phys_mem_addr((unsigned long)&ivt_start),
                             Page::Attr(Page::Rights::RWX(),
                             Page::Type::Normal(), Page::Kern::Global())));
  pte.write_back_if(true, Mem_unit::Asid_kernel);
}

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
  init_hyp_mode();
  bsp_init(is_boot_cpu);
}

IMPLEMENT_DEFAULT inline
void
Cpu::init_hyp_mode()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !arm_lpae]:

PUBLIC static inline unsigned Cpu::phys_bits() { return 32; }

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_lpae]:

PUBLIC static inline unsigned Cpu::phys_bits() { return 40; }

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
  assert(!((Mword)&monitor_vector_base & 31));
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

  register Mword r0 asm("r0") = (Mword)nonsecure_state;
  register Mword r1 asm("r1") = (Mword)go_nonsecure;

  asm volatile("stmdb sp!, {r0}   \n"
               "mov    r2, sp     \n" // copy sp_svc to sp_mon
               "cps    #0x16      \n" // switch to monitor mode
               "mov    sp, r2     \n"
               "adr    r3, 1f     \n" // save return eip
               "mrs    r4, cpsr   \n" // save return psr
               "mov    pc, r1     \n" // go nonsecure!
               "1:                \n"
               "mov    r0, sp     \n" // copy sp_mon to sp_svc
               "cps    #0x13      \n" // switch to svc mode
               "mov    sp, r0     \n"
               "ldmia  sp!, {r0}  \n"
               : : "r" (r0), "r" (r1)
               : "r2", "r3", "r4", "r5", "r6", "r7",
                 "r8", "r9", "r10", "r11", "r12", "r14", "memory");
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
IMPLEMENTATION [!debug]:

PUBLIC inline
void
Cpu::print_infos() const
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug && arm_v6plus]:

PRIVATE
void
Cpu::id_print_infos() const
{
  printf("ID_PFR[01]:  %08lx %08lx", _cpu_id._pfr[0], _cpu_id._pfr[1]);
  printf(" ID_[DA]FR0: %08lx %08lx\n", _cpu_id._dfr0, _cpu_id._afr0);
  printf("ID_MMFR[04]: %08lx %08lx %08lx %08lx\n",
         _cpu_id._mmfr[0], _cpu_id._mmfr[1], _cpu_id._mmfr[2], _cpu_id._mmfr[3]);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug && !arm_v6plus]:

PRIVATE
void
Cpu::id_print_infos() const
{
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Cpu::print_infos() const
{
  printf("Cache config: %s\n", Config::Cache_enabled ? "ON" : "OFF");
  id_print_infos();
}
