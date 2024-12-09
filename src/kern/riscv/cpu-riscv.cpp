INTERFACE [riscv]:

#include "types.h"
#include "csr.h"
#include "per_cpu_data.h"
#include "processor.h"

#include "warn.h"

EXTENSION
class Cpu
{
public:
  enum Isa_ext : Unsigned32 {
    // IDs 0-25 are reserved for single-letter RISC-V ISA extensions.
    Isa_ext_a = ('a' - 'a'), // Atomics
    Isa_ext_b = ('b' - 'a'), // Bit Manipulation
    Isa_ext_c = ('c' - 'a'), // Quad-Precision Floating-Point
    Isa_ext_d = ('d' - 'a'), // Double-Precision Floating-Point
    Isa_ext_e = ('e' - 'a'), // Reduced Integer
    Isa_ext_f = ('f' - 'a'), // Single-Precision Floating-Point
    Isa_ext_g = ('g' - 'a'), // General
    Isa_ext_h = ('h' - 'a'), // Hypervisor
    Isa_ext_i = ('i' - 'a'), // Integer
    Isa_ext_m = ('m' - 'a'), // Integer Multiplication and Division
    Isa_ext_p = ('p' - 'a'), // Packed-SIMD Extensions
    Isa_ext_q = ('q' - 'a'), // Quad-Precision Floating-Point
    Isa_ext_v = ('v' - 'a'), // Vector

    // IDs starting from 26 represent multi-letter extensions. The assignment does
    // not follow a defined order, IDs are simply assigned incrementally when new
    // extensions are added.
    Isa_ext_base = 26,

    Isa_ext_sstc = 27, // stimecmp / vstimecmp

    // Maximum number of extensions that can be represented, corresponds to the
    // size of the `Platform_info.arch.isa_ext` bitmap.
    Isa_ext_max  = 224,
  };

  enum : Mword {
    Msb = static_cast<Mword>(1) << (MWORD_BITS - 1),
  };

  enum : Mword {
    Stvec_mode_direct   = 0,
    Stvec_mode_vectored = 1,
  };

  enum : Mword {
    Sie_ssie = 1 << 1,
    Sie_stie = 1 << 5,
    Sie_seie = 1 << 9,
  };

  enum : Mword {
    Sip_ssip = 1 << 1,
    Sip_stip = 1 << 5,
    Sip_seip = 1 << 9,
  };

  enum : Mword {
    Sstatus_sie  = 1u << 1,
    Sstatus_spie = 1u << 5,
    Sstatus_spp  = 1u << 8,
    Sstatus_sum  = 1u << 18,
  };

  enum : Mword {
    Int_user_software       = 0 | Msb,
    Int_supervisor_software = 1 | Msb,
    Int_user_timer          = 4 | Msb,
    Int_supervisor_timer    = 5 | Msb,
    Int_user_external       = 8 | Msb,
    Int_supervisor_external = 9 | Msb,
  };

  enum : Mword {
    Exc_inst_misaligned  = 0,
    Exc_inst_access      = 1,
    Exc_illegal_inst     = 2,
    Exc_breakpoint       = 3,
    Exc_load_acesss      = 5,
    Exc_store_acesss     = 7,
    Exc_ecall            = 8,
    Exc_inst_page_fault  = 12,
    Exc_load_page_fault  = 13,
    Exc_store_page_fault = 15,
  };

  enum : Mword {
    Fs_off     = 0u << 13,
    Fs_initial = 1u << 13,
    Fs_clean   = 2u << 13,
    Fs_dirty   = 3u << 13,
    Fs_mask    = Fs_dirty,
  };

  enum : Mword
  {
    /// Sstatus bits that are controllable from user-mode (currently none).
    Sstatus_user_mask = 0UL,
    /// Sstatus sits that are set for user mode context by default.
    Sstatus_user_default = Sstatus_spie,
  };

  enum : Mword
  {
    Scounteren_cy = 1UL << 0,
    Scounteren_tm = 1UL << 1,
    Scounteren_ir = 1UL << 2,
  };

  void init(bool is_boot_cpu);

  static Per_cpu<Cpu> cpus;
  static Cpu *boot_cpu() { return _boot_cpu; }

  Cpu(Cpu_number id) { set_id(id); }

  static constexpr Mword STACK_ALIGNMENT = 16;

  static constexpr inline Mword stack_align(Mword stack)
  {
    return stack & ~(STACK_ALIGNMENT - 1);
  }

  static constexpr inline Mword is_stack_aligned(Mword stack)
  {
    return !(stack & (STACK_ALIGNMENT - 1));
  }

  static constexpr inline Mword stack_round(Mword size)
  {
    return (size + STACK_ALIGNMENT - 1) & ~(STACK_ALIGNMENT - 1);;
  }

  class User_mem_access
  {
    public:
      User_mem_access(bool permit = true)
      {
        _prev = permit_user_mem_access(permit);
      }

      ~User_mem_access()
      {
        permit_user_mem_access(_prev);
      }
    private:
      bool _prev;
  };

private:
  void init_hypervisor_ext();

  static Cpu *_boot_cpu;

  Proc::Hart_context _hart_context;
};

class Hart_mask
{
public:
  Hart_mask() = default;

  explicit Hart_mask(Cpu_phys_id cpu)
  {
    set(cpu);
  }

  explicit Hart_mask(Cpu_mask cpu_mask)
  {
    for (Cpu_number n = Cpu_number::first(); n < Config::max_num_cpus(); ++n)
    {
      if (cpu_mask.get(n))
        set(Cpu::cpus.cpu(n).phys_id());
    }
  }

  bool empty()
  {
    return !_hart_mask;
  }

  void set(Cpu_phys_id cpu)
  {
    _hart_mask |= 1UL << cxx::int_value<Cpu_phys_id>(cpu);
  }

  void clear(Cpu_phys_id cpu)
  {
    _hart_mask &= ~(1UL << cxx::int_value<Cpu_phys_id>(cpu));
  }

  operator Mword () const { return _hart_mask; }

private:
  Mword _hart_mask = 0;
  static_assert(sizeof(_hart_mask) * 8 >= Config::Max_num_cpus,
                "Hart mask does not fit into a single machine word.");
};

//----------------------------------------------------------------------------
INTERFACE [riscv && 32bit]:

EXTENSION
class Cpu
{
public:
  enum : Mword {
    Satp_mode_sv32  = Msb,
    Satp_mode       = Satp_mode_sv32,

    Satp_ppn_bits   = 22,

    Satp_asid_shift = Satp_ppn_bits,
    Satp_asid_bits  = 9,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && 64bit]:

EXTENSION
class Cpu
{
public:
  enum : Mword {
    Satp_mode_sv39  = 8UL << 60,
    Satp_mode_sv48  = 9UL << 60,

    Satp_ppn_bits   = 44,

    Satp_asid_shift = Satp_ppn_bits,
    Satp_asid_bits  = 16,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && riscv_sv39]:

EXTENSION
class Cpu
{
public:
  enum : Mword {
    Satp_mode       = Satp_mode_sv39,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && riscv_sv48]:

EXTENSION
class Cpu
{
public:
  enum : Mword {
    Satp_mode       = Satp_mode_sv48,
  };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "kip.h"

#include <cassert>

DEFINE_PER_CPU_P(0) Per_cpu<Cpu> Cpu::cpus(Per_cpu_data::Cpu_num);
Cpu *Cpu::_boot_cpu;

PUBLIC inline
Cpu_phys_id
Cpu::phys_id() const
{ return _hart_context._phys_id; }

IMPLEMENT
void
Cpu::init(bool is_boot_cpu)
{
  if (is_boot_cpu)
    {
      _boot_cpu = this;
      set_present(1);
      set_online(1);
    }

  _hart_context._phys_id = Proc::cpu_id();
  _hart_context._cpu_id = id();
  Proc::hart_context(&_hart_context);

  init_trap_handler();
  init_hypervisor_ext();
}

PUBLIC static inline
bool
Cpu::have_superpages()
{ return true; }

PUBLIC static inline NEEDS ["kip.h"]
bool
Cpu::has_isa_ext(Isa_ext ext)
{
  if (ext < 0 || ext >= Isa_ext_max)
    return 0;

  return Kip::k()->platform_info.arch.isa_ext[ext / 32] & (1 << (ext % 32));
}

PUBLIC
bool
Cpu::if_show_infos() const
{
  return id() == Cpu_number::boot_cpu() || !boot_cpu();
}

PUBLIC
void
Cpu::print_infos() const
{
  if (if_show_infos())
    printf("CPU[%u]: Extensions: %x\n",
           cxx::int_value<Cpu_number>(id()),
           Kip::k()->platform_info.arch.isa_ext[0]);
}

extern "C" void handle_trap(void);

PRIVATE static
void
Cpu::init_trap_handler()
{
  // Set sscratch to zero to indicate to the trap handler
  // that we are currently executing in the kernel.
  csr_write(sscratch, 0);
  set_stvec(Stvec_mode_direct, reinterpret_cast<Mword>(&handle_trap));

  // Enable Supervisor Software Interrupts (IPI) and External Interrupts (PLIC)
  csr_set(sie, Sie_ssie | Sie_seie);
}

IMPLEMENT_DEFAULT inline
void
Cpu::init_hypervisor_ext()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

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
Unsigned64
Cpu::rdtsc (void)
{ return 0; }

PUBLIC static inline ALWAYS_INLINE
Mword
Cpu::phys_to_ppn(Address phys)
{
  return phys >> 12;
}

PUBLIC static inline ALWAYS_INLINE
Address
Cpu::ppn_to_phys(Mword ppn)
{
  return ppn << 12;
}

PUBLIC static inline ALWAYS_INLINE
Mword
Cpu::get_satp()
{
  return csr_read(satp);
}

PUBLIC static inline ALWAYS_INLINE
void
Cpu::set_satp_unchecked(Mword asid, Mword ppn)
{
  csr_write(satp, Satp_mode | (asid << Satp_asid_shift) | ppn);
}

PUBLIC static inline NEEDS[<cassert>]
void
Cpu::set_satp(Mword asid, Mword ppn)
{
  assert(ppn < (1UL << Satp_asid_shift));
  assert(asid == (asid & ((1 << Satp_asid_bits) - 1)));

  set_satp_unchecked(asid, ppn);
}

PUBLIC static inline
Mword
Cpu::get_asid()
{
  return cxx::get_lsb(csr_read(satp) >> Satp_asid_shift, Satp_asid_bits);
}

PUBLIC static inline
void
Cpu::set_asid(Mword asid)
{
  Mword ppn = cxx::get_lsb(get_satp(), Satp_ppn_bits);
  set_satp(asid, ppn);
}

PUBLIC static inline NEEDS[<cassert>]
void
Cpu::set_stvec(Mword mode, Mword base_address)
{
  assert(!(base_address & 3));
  csr_write(stvec, base_address | mode);
}

PUBLIC static inline
void
Cpu::enable_timer_interrupt(bool enable)
{
  if (enable)
    csr_set(sie, Sie_stie);
  else
    csr_clear(sie, Sie_stie);
}

PUBLIC static inline
bool
Cpu::permit_user_mem_access(bool permit)
{
  if (permit)
    return csr_read_set(sstatus, Sstatus_sum) & Sstatus_sum;
  else
    return csr_read_clear(sstatus, Sstatus_sum) & Sstatus_sum;
}

PUBLIC static inline
void
Cpu::clear_software_interrupt()
{
  csr_clear(sip, Cpu::Sip_ssip);
}

PUBLIC static inline
Unsigned32
Cpu::inst_size(Address inst_addr)
{
  Mword inst = *reinterpret_cast<Unsigned16 *>(inst_addr);
  // Non-compressed instructions have the two least-significant bits set.
  bool compressed = (inst & 0b11) != 0b11;
  return compressed ? 2 : 4;
}

//----------------------------------------------------------------------------
IMPLEMENTATION[riscv && 32bit]:

PUBLIC static inline unsigned Cpu::phys_bits() { return 34; }

PUBLIC static inline
Unsigned64
Cpu::rdtime()
{
  Unsigned32 hi, lo;
  do
    {
      hi = csr_read(timeh);
      lo = csr_read(time);
    }
  while (hi != csr_read(timeh));

  return static_cast<Unsigned64>(hi) << 32 | lo;
}

PUBLIC static inline
Unsigned64
Cpu::rdcycle()
{
  Unsigned32 hi, lo;
  do
    {
      hi = csr_read(cycleh);
      lo = csr_read(cycle);
    }
  while (hi != csr_read(cycleh));

  return static_cast<Unsigned64>(hi) << 32 | lo;
}

//----------------------------------------------------------------------------
IMPLEMENTATION[riscv && 64bit]:

PUBLIC static inline unsigned Cpu::phys_bits() { return 56; }

PUBLIC static inline
Unsigned64
Cpu::rdtime()
{
  return csr_read(time);
}

PUBLIC static inline
Unsigned64
Cpu::rdcycle()
{
  return csr_read(cycle);
}

//----------------------------------------------------------------------------
IMPLEMENTATION[riscv && riscv_sv39]:

IMPLEMENT_OVERRIDE inline
bool
Cpu::is_canonical_address(Address virt)
{
  // A canonical virtual address according to Sv39 must have
  // bits 39..63 all equal to bit 38.
  return virt >= 0xffffffc000000000UL || virt <= 0x0000003fffffffffUL;
}

//----------------------------------------------------------------------------
IMPLEMENTATION[riscv && riscv_sv48]:

IMPLEMENT_OVERRIDE inline
bool
Cpu::is_canonical_address(Address virt)
{
  // A canonical virtual address according to Sv48 must have
  // bits 48..63 all equal to bit 47.
  return virt >= 0xffff800000000000UL || virt <= 0x00007fffffffffffUL;
}
