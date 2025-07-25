INTERFACE[ia32 || amd64]:

#include "asm.h"
#include "gdt.h"
#include "l4_types.h"
#include "types.h"
#include "initcalls.h"
#include "msrdefs.h"
#include "regdefs.h"
#include "per_cpu_data.h"

#define FIASCO_IA32_LOAD_SEG_SAFE(seg, val) \
  asm volatile ("mov %0, %%" #seg : : "rm"(val))

#define FIASCO_IA32_LOAD_SEG(seg, val) \
  asm volatile (                              \
    "1: mov %0, %%" #seg "\n\t"               \
    ".pushsection \".fixup.%=\", \"ax?\"\n\t" \
    "2: movw  $0, %0                    \n\t" \
    "   jmp 1b                          \n\t" \
    ".popsection                        \n\t" \
    ASM_KEX(1b, 2b)                           \
    : : "rm" (val))

class Gdt;
class Tss;

EXTENSION
class Cpu
{
  friend class Kip_test;

public:

  enum Vendor
  {
    Vendor_unknown = 0,
    Vendor_intel,
    Vendor_amd,
  };

  enum
  {
    Ldt_entry_size = 8,
  };

  enum Local_features
  {
    Lf_rdpmc            = 1U << 0,  // supports RDPMC instruction
    Lf_rdpmc32          = 1U << 1,  // supports RDPMC32 instruction
    Lf_tsc_invariant    = 1U << 2,  // TSC runs at constant rate and does not
                                    // stop in any ACPI state
  };

  Unsigned64 time_us() const;
  int can_wrmsr() const;

private:
  void init(Cpu_number cpu);
  Unsigned64 _frequency;
  Unsigned32 _version;                  // CPUID(1).EAX
  Unsigned32 _brand;                    // CPUID(1).EBX
  Unsigned32 _ext_features;             // CPUID(1).ECX
  Unsigned32 _features;                 // CPUID(1).EDX
  Unsigned32 _ext_07_ebx;               // CPUID(7,0).EBX
  Unsigned32 _ext_07_edx;               // CPUID(7,0).EDX
  Unsigned32 _ext_8000_0001_ecx;        // CPUID(8000_0001H).ECX
  Unsigned32 _ext_8000_0001_edx;        // CPUID(8000_0001H).EDX
  Unsigned32 _local_features;           // See Local_features
  Unsigned64 _arch_capabilities;        // Msr::Ia32_arch_capabilities

  Unsigned8 _phys_bits;
  Unsigned8 _virt_bits;

  Vendor _vendor;

  Unsigned32 _arch_perfmon_info_eax;    // CPUID(0AH).EAX
  Unsigned32 _arch_perfmon_info_ebx;    // CPUID(0AH).EBX
  Unsigned32 _arch_perfmon_info_ecx;    // CPUID(0AH).ECX
  Unsigned32 _arch_perfmon_info_edx;    // CPUID(0AH).EDX

  Unsigned32 _monitor_mwait_eax;        // CPUID(5).EAX
  Unsigned32 _monitor_mwait_ebx;        // CPUID(5).EBX
  Unsigned32 _monitor_mwait_ecx;        // CPUID(5).ECX
  Unsigned32 _monitor_mwait_edx;        // CPUID(5).EDX

  Unsigned32 _thermal_and_pm_eax;       // CPUID(6).EAX

  static Unsigned32 scaler_tsc_to_ns;
  static Unsigned32 scaler_tsc_to_us;
  static Unsigned32 scaler_ns_to_tsc;

public:

  void disable(Cpu_number cpu, char const *reason);

  Vendor vendor() const { return _vendor; }

  unsigned family() const
  { return (_version >> 8 & 0xf) + (_version >> 20 & 0xff); }

  unsigned model() const
  { return (_version >> 4 & 0xf) + (_version >> 12 & 0xf0); }

  unsigned stepping() const { return _version & 0xF; }
  unsigned type() const { return (_version >> 12) & 0x3; }
  Unsigned64 frequency() const { return _frequency; }
  unsigned brand() const { return _brand & 0xFF; }
  unsigned features() const { return _features; }
  unsigned ext_features() const { return _ext_features; }
  bool has_monitor_mwait() const { return _ext_features & FEATX_MONITOR; }
  bool has_monitor_mwait_irq() const { return _monitor_mwait_ecx & 3; }
  bool has_pcid() const { return _ext_features & FEATX_PCID; }
  unsigned arch_perf_mon_version() const { return _arch_perfmon_info_eax & 0xff; }

  bool __attribute__((const)) has_smep() const
  { return _ext_07_ebx & FEATX_SMEP; }

  bool __attribute__((const)) has_invpcid() const
  { return _ext_07_ebx & FEATX_INVPCID; }

  bool __attribute__((const)) has_l1d_flush() const
  { return (_ext_07_edx & FEATX_L1D_FLUSH); }

  bool __attribute__((const)) has_arch_capabilities() const
  { return (_ext_07_edx & FEATX_IA32_ARCH_CAPABILITIES); }

  bool __attribute ((const)) skip_l1dfl_vmentry() const
  { return (_arch_capabilities & (1UL << 3)); }

  unsigned ext_8000_0001_ecx() const { return _ext_8000_0001_ecx; }
  unsigned ext_8000_0001_edx() const { return _ext_8000_0001_edx; }
  unsigned local_features() const { return _local_features; }
  bool superpages() const { return features() & FEAT_PSE; }
  bool tsc() const { return features() & FEAT_TSC; }
  bool sysenter() const { return features() & FEAT_SEP; }
  bool syscall() const { return ext_8000_0001_edx() & FEATA_SYSCALL; }
  bool vmx() { return boot_cpu()->ext_features() & FEATX_VMX; }
  bool svm() { return boot_cpu()->vendor() == Vendor_amd
                      && boot_cpu()->ext_8000_0001_ecx() & FEATA_SVM; }
  bool has_amd_osvw() { return  boot_cpu()->ext_8000_0001_ecx() & (1<<9); }
  unsigned virt_bits() const { return _virt_bits; }
  unsigned phys_bits() const { return _phys_bits; }
  Unsigned32 get_scaler_tsc_to_ns() const { return scaler_tsc_to_ns; }
  Unsigned32 get_scaler_tsc_to_us() const { return scaler_tsc_to_us; }
  Unsigned32 get_scaler_ns_to_tsc() const { return scaler_ns_to_tsc; }

  Address volatile &kernel_sp() const;

public:
  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  static bool have_superpages() { return boot_cpu()->superpages(); }
  static bool have_sysenter() { return boot_cpu()->sysenter(); }
  static bool have_syscall()
  {
    if constexpr (sizeof(long) == 4)
      if (boot_cpu()->_vendor == Vendor_intel)
        return false;
    return boot_cpu()->syscall();
  }
  static bool have_fxsr() { return boot_cpu()->features() & FEAT_FXSR; }
  static bool have_pge() { return boot_cpu()->features() & FEAT_PGE; }
  static bool have_xsave() { return boot_cpu()->ext_features() & FEATX_XSAVE; }

  bool has_xsave() const { return ext_features() & FEATX_XSAVE; }

private:
  static Cpu *_boot_cpu;
  static char const * const vendor_ident[];
  static char const * const exception_strings[];

public:
  enum Lbr
  {
    Lbr_uninitialized = 0,
    Lbr_unsupported,
    Lbr_pentium_6,
    Lbr_pentium_4,
    Lbr_pentium_4_ext,
  };

  enum Bts
  {
    Bts_uninitialized = 0,
    Bts_unsupported,
    Bts_pentium_m,
    Bts_pentium_4,
  };


  enum Xstate : Unsigned64
  {
    Xstate_fp           = 1 << 0,
    Xstate_sse          = 1 << 1,
    Xstate_avx          = 1 << 2,
    Xstate_avx512       = 0x7 << 5,
    Xstate_defined_bits = Xstate_fp | Xstate_sse | Xstate_avx | Xstate_avx512,
  };

private:
  /** Flags if lbr or bts facilities are activated, used by double-fault
   *  handler to reset the debugging facilities
   */
  Unsigned32 debugctl_busy;

  /** debugctl value for activating lbr or bts */
  Unsigned32 debugctl_set;

  /** debugctl value to reset activated lr/bts facilities in the double-fault
   *  handler
   */
  Unsigned32 debugctl_reset;

  /** supported lbr type */
  Lbr _lbr;

  /** supported bts type */
  Bts _bts;

  /** is lbr active ? */
  char lbr_active;

  /** is btf active ? */
  char btf_active;

  /** is bts active ? */
  char bts_active;

  Gdt *gdt;
  Tss *_tss;
  Tss *_tss_dbf;

public:
  Lbr lbr_type() const { return _lbr; }
  Bts bts_type() const { return _bts; }
  bool lbr_status() const { return lbr_active; }
  bool bts_status() const { return bts_active; }
  bool btf_status() const { return btf_active; }

  Gdt *get_gdt() const { return gdt; }
  Tss *get_tss() const { return _tss; }

  void set_gdt() const
  {
    Pseudo_descriptor desc(reinterpret_cast<Address>(gdt), Gdt::gdt_max-1);
    Gdt::set (&desc);
  }

  static void set_tss() { set_tr(Gdt::gdt_tss); }

  /// Return the CPU's microcode revision
  static Unsigned32 ucode_revision()
  {
    Unsigned32 a, b, c, d;
    Cpu::wrmsr(0, Msr::Ia32_bios_sign_id);
    Cpu::cpuid(1, &a, &b, &c, &d);
    return Cpu::rdmsr(Msr::Ia32_bios_sign_id) >> 32;
  }

private:
  void init_lbr_type();
  void init_bts_type();

  Unsigned64 _suspend_tsc;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include <cstdio>
#include <cstring>
#include "asm.h"
#include "config.h"
#include "div32.h"
#include "gdt.h"
#include "globals.h"
#include "initcalls.h"
#include "io.h"
#include "lock_guard.h"
#include "panic.h"
#include "pit.h"
#include "processor.h"
#include "regdefs.h"
#include "spin_lock.h"
#include "tss.h"
#include "warn.h"

struct Ia32_intel_microcode
{
  struct Ext_signature
  {
    Unsigned32 signature;
    Unsigned32 processor_flags;
    Unsigned32 checksum;
  } __attribute__((packed));

  struct Ext_signature_table
  {
    Unsigned32 count;
    Unsigned32 checksum;
    char _reserved[12];
    Ext_signature sig[];

    bool checksum_valid() const
    {
      Unsigned32 const *w = &count;
      Unsigned32 const *e = w + (count * 3) + 5;
      Unsigned32 cs = 0;
      for (; w < e; ++w)
        cs += w[0];

      return cs == 0;
    }

  } __attribute__((packed));

  struct Header
  {
    Unsigned32 hdr_version;
    Signed32   update_rev;
    Unsigned16 date_year;
    Unsigned8  date_day;
    Unsigned8  date_month;
    Unsigned32 signature;
    Unsigned32 checksum;
    Unsigned32 loader_rev;
    Unsigned32 processor_flags;
    Unsigned32 _data_size;
    Unsigned32 _total_size;
    char _reserved[12];

    Unsigned32 data_size() const
    { return _data_size ? _data_size : 2000; }

    void const *data() const { return this + 1; }

    Unsigned32 total_size() const
    {
      static_assert (sizeof(Header) == 48,
                     "invalid size for microcode header");
      return _data_size ? _total_size : 2048;
    }

    bool checksum_valid() const
    {
      // must be a multiple of 1KiB
      if (total_size() & 0x3ff)
        return false;

      Unsigned32 const *w = &hdr_version;
      Unsigned32 const *e = w + (total_size() / 4);
      Unsigned32 cs = 0;
      for (; w < e; ++w)
        cs += w[0];

      return cs == 0;
    }

    bool match_proc(Unsigned32 sig, Unsigned32 proc_mask) const
    {
      if ((sig == signature)
          && (processor_flags & proc_mask))
        return true;

      if (total_size() <= (data_size() + 48 + 20))
        return false;

      auto *et = offset_cast<Ext_signature_table const *>(this + 1, data_size());

      if (!et->checksum_valid())
        return false;

      for (auto const *e = et->sig; e != et->sig + et->count; ++e)
        {
          if ((e->signature == sig)
              && (e->processor_flags & proc_mask))
            return true;
        }

      return false;
    }

    bool match(Unsigned64 rev_sig, Unsigned32 proc_mask) const
    {
      if (!match_proc(rev_sig & 0xffffffffU, proc_mask))
        return false;

      return static_cast<Signed32>(rev_sig >> 32) < update_rev;
    }

  } __attribute__((packed));

  static Unsigned64 get_sig()
  {
    Cpu::wrmsr(0, Msr::Ia32_bios_sign_id);
    Unsigned32 a = Cpu::cpuid_eax(1);
    return (Cpu::rdmsr(Msr::Ia32_bios_sign_id) & 0xffffffff00000000) | a;
  }

  static Header const *find(Unsigned64 rev_sig)
  {
    // get platform ID from IA32_PLATFORM_ID msr
    Unsigned32 proc_mask = 1U << ((Cpu::rdmsr(Msr::Ia32_platform_id) >> 50) & 0x7);

    extern char const __attribute__((weak))ia32_intel_microcode_start[];
    extern char const __attribute__((weak))ia32_intel_microcode_end[];
    char const *pos = ia32_intel_microcode_start;

    if (reinterpret_cast<Address>(pos) & 0xf)
      {
        printf("warning: microcode updates misaligned, skipping\n");
        return nullptr;
      }

    Header const *update = nullptr;

    while (pos
           && (pos < ia32_intel_microcode_end)
           && (pos + 48 < ia32_intel_microcode_end))
      {
        auto const *u = reinterpret_cast<Header const *>(pos);
        unsigned ts = u->total_size();
        if (ts & 0x3ff)
          {
            printf("warning: microcode update size invalid: %x\n", ts);
            return nullptr;
          }

        if (pos + ts > ia32_intel_microcode_end)
          {
            printf("warning: truncated microcode update, skip\n");
            return nullptr;
          }

        if (u->loader_rev != 1)
          {
            printf("warning: microcode update, unknown loader revision: %x\n",
                   u->loader_rev);

            pos += ts;
            continue;
          }

        if (u->match(rev_sig, proc_mask))
          {
            if (!u->checksum_valid())
              printf("warning: microcode update checksum error, skipping\n");
            else if (!update || update->update_rev < u->update_rev)
              update = u;
          }

        pos += ts;
      }

    return update;
  }

  static bool load()
  {
    Unsigned64 rev_sig = get_sig();
    auto const *update = find(rev_sig);
    if (!update)
      return false;

    static Spin_lock<> load_lock;

      {
        auto g = lock_guard(load_lock);
        Cpu::wrmsr(reinterpret_cast<Address>(update->data()),
                   Msr::Ia32_bios_updt_trig);
      }

    Unsigned64 n = get_sig();
    if (rev_sig != n)
      {
        printf("microcode update: rev %llx -> %llx (%04x-%02x-%02x)\n",
               rev_sig >> 32, n >> 32, update->date_year, update->date_month,
               update->date_day);
      }
    else
      {
        printf("error: could not load microcode update: rev %llx != %llx"
               " (%04x-%02x-%02x)\n",
               rev_sig, n, update->date_year, update->date_month,
               update->date_day);
        return false;
      }
    return true;
  }
};

/**
 * Reset the IO bitmap.
 *
 * Instead of physically resetting the IO bitmap by setting its bits, the
 * IO bitmap offset in the TSS is set beyond the TSS segment limit.
 *
 * On an IO port access, this effectively causes a #GP exception even without
 * the CPU accessing the IO bitmap.
 *
 * \note This method needs to be called with the CPU lock held.
 */
PUBLIC inline NEEDS["tss.h"]
void
Cpu::reset_io_bitmap()
{
  _tss->_hw.ctx.iopb = Tss::Segment_limit + 1;
  _tss->_io_bitmap_revision = 0;
}

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;

Unsigned32 Cpu::scaler_tsc_to_ns;
Unsigned32 Cpu::scaler_tsc_to_us;
Unsigned32 Cpu::scaler_ns_to_tsc;

char const * const Cpu::vendor_ident[] =
{
  nullptr,
  "GenuineIntel",
  "AuthenticAMD",
};

char const * const Cpu::exception_strings[] =
{
  /*  0 */ "Divide Error",
  /*  1 */ "Debug",
  /*  2 */ "NMI Interrupt",
  /*  3 */ "Breakpoint",
  /*  4 */ "Overflow",
  /*  5 */ "BOUND Range Exceeded",
  /*  6 */ "Invalid Opcode",
  /*  7 */ "Device Not Available",
  /*  8 */ "Double Fault",
  /*  9 */ "CoProcessor Segment Overrrun",
  /* 10 */ "Invalid TSS",
  /* 11 */ "Segment Not Present",
  /* 12 */ "Stack Segment Fault",
  /* 13 */ "General Protection",
  /* 14 */ "Page Fault",
  /* 15 */ "Reserved",
  /* 16 */ "Floating-Point Error",
  /* 17 */ "Alignment Check",
  /* 18 */ "Machine Check",
  /* 19 */ "SIMD Floating-Point Exception",
  /* 20 */ "Virtualization Exception",
  /* 21 */ "Control Protection Exception",
  /* 22 */ "Reserved",
  /* 23 */ "Reserved",
  /* 24 */ "Reserved",
  /* 25 */ "Reserved",
  /* 26 */ "Reserved",
  /* 27 */ "Reserved",
  /* 28 */ "Reserved",
  /* 29 */ "Reserved",
  /* 30 */ "Reserved",
  /* 31 */ "Reserved"
};

PUBLIC explicit FIASCO_INIT_CPU
Cpu::Cpu(Cpu_number cpu)
{
  set_id(cpu);
  if (cpu == Cpu_number::boot_cpu())
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
    }

  init(cpu);
}


PUBLIC static
void
Cpu::init_global_features()
{}

PUBLIC static
char const *
Cpu::exception_string(Mword trapno)
{
  if (trapno > 31)
    return "Maskable Interrupt";
  return exception_strings[trapno];
}

PUBLIC static inline FIASCO_INIT_CPU_SFX(cpuid)
void
Cpu::cpuid(Unsigned32 mode, Unsigned32 ecx_val,
           Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx, Unsigned32 *edx)
{ Proc::cpuid(mode, ecx_val, eax, ebx, ecx, edx); }

PUBLIC static inline FIASCO_INIT_CPU_AND_PM
void
Cpu::cpuid(Unsigned32 mode,
           Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx, Unsigned32 *edx)
{ Proc::cpuid(mode, 0, eax, ebx, ecx, edx); }

PUBLIC static inline FIASCO_INIT_CPU_AND_PM
Unsigned32
Cpu::cpuid_eax(Unsigned32 mode)
{ return Proc::cpuid_eax(mode); }

PUBLIC static inline FIASCO_INIT_CPU_AND_PM
Unsigned32
Cpu::cpuid_ebx(Unsigned32 mode)
{ return Proc::cpuid_ebx(mode); }

PUBLIC static inline FIASCO_INIT_CPU_AND_PM
Unsigned32
Cpu::cpuid_ecx(Unsigned32 mode)
{ return Proc::cpuid_ecx(mode); }

PUBLIC static inline FIASCO_INIT_CPU_AND_PM
Unsigned32
Cpu::cpuid_edx(Unsigned32 mode)
{ return Proc::cpuid_edx(mode); }

PUBLIC
void
Cpu::update_features_info()
{
  cpuid(1, &_version, &_brand, &_ext_features, &_features);

  if (family() == 6 && model() == 0x5c) // Apollo Lake
    _ext_features &= ~FEATX_MONITOR;
}

PRIVATE FIASCO_INIT_CPU
void
Cpu::addr_size_info()
{
  Unsigned32 eax = cpuid_eax(0x80000008);

  _phys_bits = eax & 0xff;
  _virt_bits = (eax & 0xff00) >> 8;
}

PUBLIC static
unsigned
Cpu::amd_cpuid_mnc()
{
  Unsigned32 ecx = cpuid_ecx(0x80000008);

  unsigned apicidcoreidsize = (ecx >> 12) & 0xf;
  if (apicidcoreidsize == 0)
    return (ecx & 0xf) + 1; // NC
  return 1 << apicidcoreidsize;
}

PUBLIC inline FIASCO_INIT_CPU_SFX(arch_perfmon_info)
void
Cpu::arch_perfmon_info(Unsigned32 *eax, Unsigned32 *ebx, Unsigned32 *ecx,
                       Unsigned32 *edx) const
{
  *eax = _arch_perfmon_info_eax;
  *ebx = _arch_perfmon_info_ebx;
  *ecx = _arch_perfmon_info_ecx;
  *edx = _arch_perfmon_info_edx;
}

PUBLIC static
unsigned long
Cpu::get_features()
{
  Unsigned32 eflags = get_flags();
  // Check for Alignment Check Support
  set_flags(eflags ^ EFLAGS_AC);
  if (((get_flags() ^ eflags) & EFLAGS_AC) == 0)
    return 0;

  // Check for CPUID Support
  set_flags(eflags ^ EFLAGS_ID);
  if (!((get_flags() ^ eflags) & EFLAGS_ID))
    return 0;

  if (cpuid_eax(0) < 1)
    return 0;

  return cpuid_edx(1);
}


/** Identify the CPU features.
    Attention: This function may be called more than once. The reason is
    that enabling a Local APIC that was previously disabled by the BIOS
    may change the processor features. Therefore, this function has to
    be called again after the Local APIC was enabled.
 */
PUBLIC FIASCO_INIT_CPU
void
Cpu::identify()
{
  Unsigned32 eflags = get_flags();

  _phys_bits = 32;
  _virt_bits = 32;

  // Check for Alignment Check Support -- works only on 486 and later
  set_flags(eflags ^ EFLAGS_AC);
  // FIXME: must not panic at cpu hotplug
  if (((get_flags() ^ eflags) & EFLAGS_AC) == 0)
    panic("CPU too old");

  // Check for CPUID Support
  set_flags(eflags ^ EFLAGS_ID);
  if ((get_flags() ^ eflags) & EFLAGS_ID)
    {
      Unsigned32 max, i;
      char vendor_id[12];
      get_vendor_str_current_cpu(vendor_id);

      for (i = cxx::size(vendor_ident) - 1; i; --i)
        if (!memcmp(vendor_id, vendor_ident[i], 12))
          break;

      _vendor = static_cast<Cpu::Vendor>(i);

      if (_vendor == Vendor_intel)
        Ia32_intel_microcode::load();

      init_indirect_branch_mitigation();

      max = cpuid_eax(0);

      if (max >= 1)
        update_features_info();

      if (max >= 5 && has_monitor_mwait())
        cpuid(5, &_monitor_mwait_eax, &_monitor_mwait_ebx,
              &_monitor_mwait_ecx, &_monitor_mwait_edx);

      if (max >= 6 && _vendor == Vendor_intel)
        _thermal_and_pm_eax = cpuid_eax(6);

      try_enable_hw_performance_states(false);

      if (max >= 7 && _vendor == Vendor_intel)
        {
          Unsigned32 dummy1, dummy2;
          cpuid(0x7, 0, &dummy1, &_ext_07_ebx, &dummy2, &_ext_07_edx);
          if (has_arch_capabilities())
            _arch_capabilities = rdmsr(Msr::Ia32_arch_capabilities);
        }

      if (max >= 10 && _vendor == Vendor_intel) // CPUID(0AH) reserved on AMD
        cpuid(10, &_arch_perfmon_info_eax, &_arch_perfmon_info_ebx,
              &_arch_perfmon_info_ecx, &_arch_perfmon_info_edx);

      if (_vendor == Vendor_intel)
        {
          switch (family())
            {
            case 5:
            // Avoid Pentium Erratum 74
            if ((_features & FEAT_MMX) &&
                (model() != 4 ||
                 (stepping() != 4 && (stepping() != 3 || type() != 1))))
              _local_features |= Lf_rdpmc;
            break;
            case 6:
            // Avoid Pentium Pro Erratum 26
            if (model() >= 3 || stepping() > 9)
              _local_features |= Lf_rdpmc;
            break;
            case 15:
            _local_features |= Lf_rdpmc;
            _local_features |= Lf_rdpmc32;
            break;
            }
        }
      else if (_vendor == Vendor_amd)
        {
          switch (family())
            {
            case 6:
            case 15:
            _local_features |= Lf_rdpmc;
            break;
            }
        }

      max = cpuid_eax(0x80000000);
      if (max > 0x80000000)
        {
          switch (max)
            {
            default:
            [[fallthrough]];
            case 0x80000008:
            if (_vendor == Vendor_amd || _vendor == Vendor_intel)
              addr_size_info();
            [[fallthrough]];
            case 0x80000007:
            if (_vendor == Vendor_amd || _vendor == Vendor_intel)
              if (cpuid_edx(0x80000007) & (1U << 8))
                _local_features |= Lf_tsc_invariant;
            [[fallthrough]];
            case 0x80000003:
            case 0x80000002:
            case 0x80000001:
            if (_vendor == Vendor_intel || _vendor == Vendor_amd)
              cpuid(0x80000001, &i, &i, &_ext_8000_0001_ecx,
                    &_ext_8000_0001_edx);
            break;
            }
        }

      // see Intel Spec on SYSENTER:
      // Some Pentium Pro pretend to have it, but actually lack it
      if ((_version & 0xFFF) < 0x633)
        _features &= ~FEAT_SEP;

    }
  else
    _version = 0x400; // 486

  set_flags(eflags);
}

PUBLIC inline NEEDS["processor.h"]
void
Cpu::busy_wait_ns(Unsigned64 ns)
{
  Unsigned64 stop = rdtsc () + ns_to_tsc(ns);

  while (rdtsc() < stop)
    Proc::pause();
}

IMPLEMENT
void
Cpu::disable(Cpu_number cpu, char const *reason)
{
  printf("CPU%u: is disabled: %s\n", cxx::int_value<Cpu_number>(cpu), reason);
}

// Function used for calculating apic scaler
PUBLIC static inline
Unsigned32
Cpu::muldiv(Unsigned32 val, Unsigned32 mul, Unsigned32 div)
{
  Unsigned32 dummy;

  asm volatile ("mull %3 ; divl %4\n\t"
               :"=a" (val), "=d" (dummy)
               : "0" (val),  "d" (mul),  "c" (div));
  return val;
}


PUBLIC static inline
Unsigned16
Cpu::get_cs()
{
  Unsigned16 val;
  asm volatile ("mov %%cs, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
Unsigned16
Cpu::get_ds()
{
  Unsigned16 val;
  asm volatile ("mov %%ds, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
Unsigned16
Cpu::get_es()
{
  Unsigned16 val;
  asm volatile ("mov %%es, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline
Unsigned16
Cpu::get_ss()
{
  Unsigned16 val;
  asm volatile ("mov %%ss, %0" : "=rm" (val));
  return val;
}

PUBLIC static inline NEEDS["asm.h"]
void
Cpu::set_ds(Unsigned16 val)
{
  if (__builtin_constant_p(val))
    FIASCO_IA32_LOAD_SEG_SAFE(ds, val);
  else
    FIASCO_IA32_LOAD_SEG(ds, val);
}

PUBLIC static inline NEEDS["asm.h"]
void
Cpu::set_es(Unsigned16 val)
{
  if (__builtin_constant_p(val))
    FIASCO_IA32_LOAD_SEG_SAFE(es, val);
  else
    FIASCO_IA32_LOAD_SEG(es, val);
}

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::pm_suspend()
{
  Gdt_entry tss_entry = (*gdt)[Gdt::gdt_tss / 8];

  tss_entry.tss_make_available();
  (*gdt)[Gdt::gdt_tss / 8] = tss_entry;
  _suspend_tsc = rdtsc();
}

PUBLIC FIASCO_INIT_AND_PM
void
Cpu::pm_resume()
{
  set_gdt();
  set_ldt(0);

  set_ds(Gdt::data_segment());
  set_es(Gdt::data_segment());
  set_ss(Gdt::gdt_data_kernel | Gdt::Selector_kernel);
  set_fs(Gdt::gdt_data_user   | Gdt::Selector_user);
  set_gs(Gdt::gdt_data_user   | Gdt::Selector_user);
  set_cs();

  // the boot CPU restores TSS in asm already
  if (id() != Cpu_number::boot_cpu())
    set_tss();

  if (_vendor == Vendor_intel)
    Ia32_intel_microcode::load();

  init_indirect_branch_mitigation();

  init_sysenter();

  if ((features() & FEAT_TSC) && can_wrmsr())
    if (_ext_07_ebx & FEATX_IA32_TSC_ADJUST)
      wrmsr(0, 0, Msr::Ia32_tsc_adjust);

  try_enable_hw_performance_states(true);
}

PUBLIC static inline
void
Cpu::set_cr0(unsigned long val)
{ asm volatile ("mov %0, %%cr0" : : "r" (val)); }

PUBLIC static inline
void
Cpu::set_pdbr(unsigned long addr)
{ asm volatile ("mov %0, %%cr3" : : "r" (addr)); }

PUBLIC static inline
void
Cpu::set_cr4(unsigned long val)
{ asm volatile ("mov %0, %%cr4" : : "r" (val)); }

PUBLIC static inline
void
Cpu::set_ldt(Unsigned16 val)
{ asm volatile ("lldt %0" : : "rm" (val)); }


PUBLIC static inline
void
Cpu::set_ss(Unsigned16 val)
{ asm volatile ("mov %0, %%ss" : : "r" (val)); }

PUBLIC static inline
void
Cpu::set_tr(Unsigned16 val)
{ asm volatile ("ltr %0" : : "rm" (val)); }

PUBLIC static inline
void
Cpu::xsetbv(Unsigned64 val, Unsigned32 xcr)
{
  asm volatile ("xsetbv" : : "a" (static_cast<Mword>(val)),
                             "d" (static_cast<Mword>(val >> 32)),
                             "c" (xcr));
}

PUBLIC static inline
Unsigned64
Cpu::xgetbv(Unsigned32 xcr)
{
  Unsigned32 eax, edx;
  asm volatile("xgetbv"
               : "=a" (eax),
                 "=d" (edx)
               : "c" (xcr));
  return eax | (Unsigned64{edx} << 32);
}

PUBLIC static inline
Mword
Cpu::get_cr0()
{
  Mword val;
  asm volatile ("mov %%cr0, %0" : "=r" (val));
  return val;
}

PUBLIC static inline
Address
Cpu::get_pdbr()
{ Address addr; asm volatile ("mov %%cr3, %0" : "=r" (addr)); return addr; }

PUBLIC static inline
Mword
Cpu::get_cr4()
{ Mword val; asm volatile ("mov %%cr4, %0" : "=r" (val)); return val; }

PUBLIC static inline
Unsigned16
Cpu::get_ldt()
{ Unsigned16 val; asm volatile ("sldt %0" : "=rm" (val)); return val; }

PUBLIC static inline
Unsigned16
Cpu::get_tr()
{ Unsigned16 val; asm volatile ("str %0" : "=rm" (val)); return val; }

IMPLEMENT inline
int
Cpu::can_wrmsr() const
{ return features() & FEAT_MSR; }

PUBLIC static inline
Unsigned64
Cpu::rdmsr(Msr reg, unsigned offs = 0)
{ return Proc::rdmsr(static_cast<Unsigned32>(reg) + offs); }

PUBLIC static inline
Unsigned64
Cpu::rdpmc(Unsigned32 idx, Unsigned32)
{
  Unsigned32 l,h;

  asm volatile ("rdpmc" : "=a" (l), "=d" (h) : "c" (idx));
  return (Unsigned64{h} << 32) + Unsigned64{l};
}

PUBLIC static inline
void
Cpu::wrmsr(Unsigned32 low, Unsigned32 high, Msr reg, unsigned offs = 0)
{
  Proc::wrmsr((Unsigned64{high} << 32) | low,
              static_cast<Unsigned32>(reg) + offs);
}

PUBLIC static inline
void
Cpu::wrmsr(Unsigned64 msr, Msr reg, unsigned offs = 0)
{ Proc::wrmsr(msr, static_cast<Unsigned32>(reg) + offs); }

PUBLIC static inline
void
Cpu::enable_rdpmc()
{ set_cr4(get_cr4() | CR4_PCE); }


IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init_lbr_type()
{
  _lbr = Lbr_unsupported;

  if (can_wrmsr())
    {
      // Intel
      if (vendor() == Vendor_intel)
        {
          if (family() == 15)
            _lbr = model() < 3 ? Lbr_pentium_4 : Lbr_pentium_4_ext; // P4
          else if (family() >= 6)
            _lbr = Lbr_pentium_6; // PPro, PIII
        }
      else if (vendor() == Vendor_amd)
        {
          if ((family() == 6) || (family() == 15))
            _lbr = Lbr_pentium_6; // K7/K8
        }
    }
}


IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init_bts_type()
{
  _bts = Bts_unsupported;

  if (can_wrmsr() && vendor() == Vendor_intel)
    {
      if (family() == 15 && (rdmsr(Msr::Ia32_misc_enable) & (1<<11)) == 0)
        _bts = Bts_pentium_4;
      if (family() == 6  && (model() == 9 || (model() >= 13 &&
              model() <= 15)))
        _bts = Bts_pentium_m;
      if (!(features() & FEAT_DS))
        _bts = Bts_unsupported;
    }
}


PUBLIC inline
void
Cpu::lbr_enable(bool on)
{
  if (lbr_type() != Lbr_unsupported)
    {
      if (on)
        {
          lbr_active    = true;
          debugctl_set |= 1;
          debugctl_busy = true;
        }
      else
        {
          lbr_active    = false;
          debugctl_set &= ~1;
          debugctl_busy = lbr_active || bts_active;
          wrmsr(debugctl_reset, Msr::Debugctla);
        }
    }
}


PUBLIC inline
void
Cpu::btf_enable(bool on)
{
  if (lbr_type() != Lbr_unsupported)
    {
      if (on)
        {
          btf_active      = true;
          debugctl_set   |= 2;
          debugctl_reset |= 2; /* don't disable bit in kernel */
          wrmsr(2, Msr::Debugctla);     /* activate _now_ */
        }
      else
        {
          btf_active    = false;
          debugctl_set &= ~2;
          debugctl_busy = lbr_active || bts_active;
          wrmsr(debugctl_reset, Msr::Debugctla);
        }
    }
}


PUBLIC
void
Cpu::bts_enable(bool on)
{
  if (bts_type() != Bts_unsupported)
    {
      if (on)
        {
          switch (bts_type())
            {
            case Bts_pentium_4: bts_active = true; debugctl_set |= 0x0c; break;
            case Bts_pentium_m: bts_active = true; debugctl_set |= 0xc0; break;
            default:;
            }
          debugctl_busy = lbr_active || bts_active;
        }
      else
        {
          bts_active = false;
          switch (bts_type())
            {
            case Bts_pentium_4: debugctl_set &= ~0x0c; break;
            case Bts_pentium_m: debugctl_set &= ~0xc0; break;
            default:;
            }
          debugctl_busy = lbr_active || bts_active;
          wrmsr(debugctl_reset, Msr::Debugctla);
        }
    }
}

PUBLIC inline
void
Cpu::debugctl_enable()
{
  if (debugctl_busy)
    wrmsr(debugctl_set, Msr::Debugctla);
}

PUBLIC inline
void
Cpu::debugctl_disable()
{
  if (debugctl_busy)
    wrmsr(debugctl_reset, Msr::Debugctla);
}

/*
 * AMD OS-Visible Workaround Information
 * print a warning if a CPU is affected by any known erratum
 */
PUBLIC
void
Cpu::print_errata()
{
  if (vendor() == Vendor_amd && has_amd_osvw() && can_wrmsr())
    {
      Unsigned16 osvw_id_length, i;
      bool affected = false;
      osvw_id_length = rdmsr(Msr::Osvw_id_length) & 0xff;

      for (i = 1; ((i - 1) * 64) < osvw_id_length; i++)
        {
          Unsigned64 osvw_msr = rdmsr(Msr::Osvw_id_length, i);
          if (osvw_msr != 0)
            {
              printf("\033[31mOSVW_MSR%d = 0x%016llx\033[m\n",
                     i, rdmsr(Msr::Osvw_id_length, i));
              affected = true;
            }
        }
      if (affected)
        printf("\033[31m#Errata known %d, affected by at least one\033[m\n",
               osvw_id_length);
    }
}

/**
 * Enable hardware controlled performance states (HWP) if available.
 *
 * HWP enables the processor to autonomously select performance states. The OS
 * can hint the CPU at the desired optimizations. For example, a system running
 * on battery may hint the CPU to optimize for low power consumption. We just
 * enable HWP and configure it to select the performance target autonomously.
 *
 * See Intel Manual Volume 3 Chapter 14.4 for details.
 */
PRIVATE FIASCO_INIT_CPU_AND_PM
void
Cpu::try_enable_hw_performance_states(bool resume)
{
  enum
  {
    HWP_SUPPORT = 1 << 7,
    HIGHEST_PERFORMANCE_SHIFT = 0,
    LOWEST_PERFORMANCE_SHIFT = 24
  };

  if (!(_thermal_and_pm_eax & HWP_SUPPORT))
    return;

  // enable
  wrmsr(0x1ULL, Msr::Hwp_pm_enable);

  // let the hardware decide on everything (autonomous operation mode)
  Unsigned64 hwp_caps = rdmsr(Msr::Hwp_capabilities);
  // Package_Control (bit 42) = 0
  // Activity_Window (bits 41:32) = 0 (auto)
  // Energy_Performance_Preference (bits 31:24) = 0x80 (default)
  // Desired_Performance (bits 23:16) = 0 (default)
  // Maximum_Performance (bits 15:8) = HIGHEST_PERFORMANCE(hwp_cap)
  // Minimum_Performance (bits 7:0) = LOWEST_PERFORMANCE(hwp_cap)
  Unsigned64 request =
    0x80ULL << 24 |
    (((hwp_caps >> HIGHEST_PERFORMANCE_SHIFT) & 0xff) << 8) |
    ((hwp_caps >> LOWEST_PERFORMANCE_SHIFT) & 0xff);
  wrmsr(request, Msr::Hwp_request);

  if (!resume && id() == Cpu_number::boot_cpu())
    printf("HWP: enabled\n");
}

IMPLEMENT FIASCO_INIT_CPU
void
Cpu::init(Cpu_number cpu)
{
  identify();

  init_lbr_type();
  init_bts_type();

  if (!tsc_frequency_from_cpuid_15h())
    calibrate_tsc();

  Unsigned32 cr4 = get_cr4();

  if (features() & FEAT_FXSR)
    cr4 |= CR4_OSFXSR;

  if (features() & FEAT_SSE)
    cr4 |= CR4_OSXMMEXCPT;

  if (has_smep())
    cr4 |= CR4_SMEP;

  set_cr4 (cr4);

  if constexpr (Config::Pcid_enabled)
    {
     if (!has_pcid())
       panic("CONFIG_IA32_PCID enabled but CPU lacks this feature");
     if (!has_invpcid())
       panic("CONFIG_IA32_PCID enabled but CPU lacks 'invpcid' instruction");
    }

  if constexpr (Config::Tsc_unified)
    if (!(_local_features & Lf_tsc_invariant))
      {
        // Cannot panic because QEMU does not support this CPU feature in TCG.
        if (cpu == Cpu_number::boot_cpu())
          WARN("CONFIG_TSC_UNIFIED set but CPU lacks this feature!\n");
      }

  if ((features() & FEAT_TSC) && can_wrmsr())
    if (_ext_07_ebx & FEATX_IA32_TSC_ADJUST)
      wrmsr(0, 0, Msr::Ia32_tsc_adjust);

  // See Attribs_enum on how PA0, PA2 and PA3 are used.
  // PA0 (used):   Write back (WB).
  // PA1 (unused): Write through (WT).
  // PA2 (used):   Write combining (WC).
  // PA3 (used:    Uncacheable (UC).
  // PA4 (unused): Write back (WB).
  // PA5 (unused): Write through (WT).
  // PA6 (unused): Uncached, can be overridden by WC in MTRRs (UC-).
  // PA7 (unused): Uncacheable (UC).
  if ((features() & FEAT_PAT) && can_wrmsr())
    wrmsr(0x00010406, 0x00070406, Msr::Ia32_pat);

  print_errata();
}

/** Determine the vendor string for the current CPU. */
PUBLIC static
void
Cpu::get_vendor_str_current_cpu(char vendor_id[12])
{
  Unsigned32 dummy;
  cpuid(0, &dummy, reinterpret_cast<Unsigned32 *>(vendor_id),
        reinterpret_cast<Unsigned32 *>(vendor_id + 8),
        reinterpret_cast<Unsigned32 *>(vendor_id + 4));
}

/** Determine the model string for the current CPU. */
PUBLIC static
int
Cpu::get_model_str_current_cpu(char model_str[52])
{
  strcpy(model_str, "Generic CPU");

  if (cpuid_eax(0x80000000) >= 0x80000004)
    {
      Unsigned32 *s = reinterpret_cast<Unsigned32 *>(model_str);
      for (unsigned i = 0; i < 3; ++i)
        cpuid(0x80000002 + i, &s[0 + 4 * i], &s[1 + 4 * i],
              &s[2 + 4 * i], &s[3 + 4 * i]);
      model_str[48] = 0;
    }

  // strip trailing spaces for printing pleasant CPU model name
  int i = strlen(model_str);
  while (i > 0 && model_str[i - 1] == ' ')
    --i;

  return i;
}

IMPLEMENT_OVERRIDE
void
Cpu::print_infos() const
{
  if (   id() == Cpu_number::boot_cpu() || !boot_cpu()
      || family()   != boot_cpu()->family()
      || model()    != boot_cpu()->model()
      || stepping() != boot_cpu()->stepping()
      || brand()    != boot_cpu()->brand())
    {
      // actually determining vendor/model for the current CPU, not "this" CPU
      char vendor_id[12];
      get_vendor_str_current_cpu(vendor_id);

      char model_str[52];
      int len_model_str = get_model_str_current_cpu(model_str);

      printf("CPU[%u]: %.*s (%X:%X:%X:%X)[%08x] Model: %.*s at %lluMHz\n",
             cxx::int_value<Cpu_number>(id()), 12, vendor_id, family(),
             model(), stepping(), brand(), _version, len_model_str, model_str,
             div32(frequency(), 1000000));
    }
}

// Return 2^32 / (tsc clocks per usec)
FIASCO_INIT_CPU
void
Cpu::calibrate_tsc()
{
  const unsigned calibrate_time = 50000 /*us*/ + 1;

  // sanity check
  if (! (features() & FEAT_TSC))
    goto bad_ctc;

  // only do once
  if (scaler_tsc_to_ns)
    {
      _frequency = ns_to_tsc(1000000000UL);
      return;
    }

  Unsigned64 tsc_start, tsc_end;
  Unsigned32 count, tsc_to_ns_div, dummy;

    {
      static Spin_lock<> _l;
      auto guard = lock_guard(_l);

      Pit::setup_channel2_to_20hz();

      tsc_start = rdtsc ();
      count = 0;
      do
        {
          count++;
        }
      while ((Io::in8 (0x61) & 0x20) == 0);
      tsc_end = rdtsc ();
    }

  // Error: ECTCNEVERSET
  if (count <= 1)
    goto bad_ctc;

  tsc_end -= tsc_start;

  // prevent overflow in division (CPU too fast)
  if (tsc_end & 0xffffffff00000000LL)
    goto bad_ctc;

  // prevent overflow in division (CPU too slow)
  if ((tsc_end & 0xffffffffL) < calibrate_time)
    goto bad_ctc;

  // tsc_to_ns_div = calibrate_time * 2^32 / tsc
  asm ("divl %2"
       :"=a" (tsc_to_ns_div), "=d" (dummy)
       :"r" (static_cast<Unsigned32>(tsc_end)), "a" (0), "d" (calibrate_time));

  // In 'A*1000/32', 'A*1000' could result in a value '>= 2^32' if A is too big
  // (CPU is too slow). Use 'A*(1000/32) = A*31.25' instead.
  scaler_tsc_to_ns  = tsc_to_ns_div * 31 + tsc_to_ns_div / 4;
  scaler_tsc_to_us  = tsc_to_ns_div;
  scaler_ns_to_tsc  = muldiv(1 << 31, static_cast<Unsigned32>(tsc_end),
                             calibrate_time * 1000 >> 1 * 1 << 5);
  if (scaler_tsc_to_ns)
    _frequency = ns_to_tsc(1000000000UL);

  return;

bad_ctc:
  if constexpr (Config::Kip_clock_uses_rdtsc)
    panic("Can't calibrate tsc");
}

/**
 * Set the scalers according to the CPU frequency.
 *
 * We divide the frequency by 4 to extend the frequency range up to 4<<32 Hz.
 * Loosing the lower 2 bits of the frequency value is to be acceptable. Modern
 * Intel/AMD CPUs could have a higher frequency than 4GHz.
 *
 *                     2^32 * 1000000000     2^(27-2) * 1000000000
 * scaler_tsc_to_ns = ------------------- = -----------------------
 *                      frequency * 2^5           frequency/4
 *
 *                     2^32 * 1000000        2^(31-2) * 1000000*2
 * scaler_tsc_to_us = ----------------    = ----------------------
 *                       frequency               frequency/4
 *
 * This function sets the scalers exactly like in calibrate_tsc, but instead of
 * using the number of ticks in 50ms, this function uses the frequency (ticks
 * per second).
 */
PRIVATE
void
Cpu::set_frequency_and_scalers(Unsigned64 freq)
{
  if (freq >= 4ULL << 32)
    panic("Frequency too high -- adapt Cpu::set_frequency_and_scalers");
  scaler_tsc_to_ns = muldiv(1 << (27 - 2), 1000000000, freq >> 2);
  scaler_tsc_to_us = muldiv(1 << (31 - 2), 1000000 << 1, freq >> 2);
  scaler_ns_to_tsc = muldiv(1 << (27 + 2), freq >> 2, 1000000000);

  _frequency = freq;
}

/**
 * Determine the frequency of the TSC.
 *
 * See Intel Manual Volume 3 Chapter 18.7.3 for details.
 *
 * CPUID(15H) is quite accurate.
 *
 * CPUID(16H) is less accurate and should probably be used for informational
 * purposes only. For instance, for a Kaby Lake processor, CPUID(16H) returned a
 * value of 2800MHz while the actual CPU frequency is 2808MHz.
 *
 * Msr::Platform_info is less accurate as well. On the mentioned Kaby Lake CPU,
 * bits 15:8 report 28 defining a frequency of 28*100=2800MHz.
 *
 * Calibrating the TSC delivers results which are still more accurate than the
 * rounded information from CPUID(16H) and Msr::Platform_info, even on QEMU.
 */
PRIVATE
bool
Cpu::tsc_frequency_from_cpuid_15h(bool check_only = false)
{
  if (_vendor != Vendor_intel || cpuid_eax(0) < 0x15 || family() != 6)
    return false;

  Unsigned32 eax, ebx, ecx, edx;
  cpuid(0x15, &eax, &ebx, &ecx, &edx);

  if (eax == 0 || ebx == 0)
    return false;

  // See Intel Manual Volume 3 Chapter 18.7.3 / table 18-85.
  Unsigned64 crystal_clock = 0;
  if (ecx != 0)
    crystal_clock = ecx;
  else if (model() == 0x55)     // Cascade Lake
    crystal_clock = 25000000;
  else if (   model() == 0x4e   // Skylake
           || model() == 0x5e   // Skylake
           || model() == 0x8e   // Coffee Lake
           || model() == 0x9e   // Coffee Lake
          )
    crystal_clock = 24000000;
  else if (model() == 0x5c)     // Atom Goldmont
    crystal_clock = 19200000;

  if (!crystal_clock)
    return false;

  if (check_only)
    return true;

  set_frequency_and_scalers((crystal_clock * ebx) / eax);
  return true;
}

PUBLIC inline
bool
Cpu::tsc_frequency_accurate()
{
  return tsc_frequency_from_cpuid_15h(true);
}

IMPLEMENT inline
Unsigned64
Cpu::time_us() const
{
  return tsc_to_us (rdtsc());
}


PUBLIC inline
void
Cpu::enable_ldt(Address addr, int size)
{
  if (!size)
    {
      get_gdt()->clear_entry(Gdt::gdt_ldt / 8);
      set_ldt(0);
    }
  else
    {
      get_gdt()->set_entry_ldt(Gdt::gdt_ldt / 8, addr, size - 1);
      set_ldt(Gdt::gdt_ldt);
    }
}


PUBLIC static inline
Unsigned16
Cpu::get_fs()
{ Unsigned16 val; asm volatile ("mov %%fs, %0" : "=rm" (val)); return val; }

PUBLIC static inline
Unsigned16
Cpu::get_gs()
{ Unsigned16 val; asm volatile ("mov %%gs, %0" : "=rm" (val)); return val; }

PUBLIC static inline NEEDS["asm.h"]
void
Cpu::set_fs(Unsigned16 val)
{
  if (__builtin_constant_p(val))
    FIASCO_IA32_LOAD_SEG_SAFE(fs, val);
  else
    FIASCO_IA32_LOAD_SEG(fs, val);
}

PUBLIC static inline NEEDS["asm.h"]
void
Cpu::set_gs(Unsigned16 val)
{
  if (__builtin_constant_p(val))
    FIASCO_IA32_LOAD_SEG_SAFE(gs, val);
  else
    FIASCO_IA32_LOAD_SEG(gs, val);
}

//----------------------------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && !intel_ia32_branch_barriers]:

PRIVATE inline FIASCO_INIT_CPU_AND_PM
void
Cpu::init_indirect_branch_mitigation()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION[(ia32 || amd64) && intel_ia32_branch_barriers]:

PRIVATE FIASCO_INIT_CPU_AND_PM
void
Cpu::init_indirect_branch_mitigation()
{
  if (_vendor == Vendor_intel)
    {
      if (cpuid_eax(0) < 7)
        panic("Intel CPU does not support IBRS, IBPB, STIBP (cpuid max < 7)");

      Unsigned32 d = cpuid_edx(7);
      if (!(d & FEATX_IBRS_IBPB))
        panic("IBRS / IBPB not supported by CPU: %x", d);

      if (!(d & FEATX_STIBP))
        panic("STIBP not supported by CPU: %x", d);

      // enable STIBP
      wrmsr(2, Msr::Ia32_spec_ctrl);
    }
  else
    panic("Kernel compiled with IBRS / IBPB, but not supported on non-Intel CPUs");
}
