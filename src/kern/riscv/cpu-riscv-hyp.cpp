INTERFACE [riscv && cpu_virt && 32bit]:

EXTENSION
class Cpu
{
public:
  // Not available on RV32.
  enum : Mword { Hstatus_vsxl = 0, };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && cpu_virt && 64bit]:

EXTENSION
class Cpu
{
public:
  enum : Mword
  {
    /// Controls the effective XLEN for VS-mode (only available on RV64 and only
    /// if supported by the hardware).
    Hstatus_vsxl = 3UL << 32,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && cpu_virt]:

EXTENSION
class Cpu
{
public:
  enum : Mword
  {
    /// Endianness of excplicit guest memory accesses
    Hstatus_vsbe = 1 << 5,
    /// Guest Virtual Address
    Hstatus_gva  = 1 << 6,
    /// Supervisor Previous Virtualization mode
    Hstatus_spv  = 1 << 7,
    /// Supervisor Previous Virtual Privilege
    Hstatus_spvp = 1 << 8,
    /// Hypervisor load/store instructions executable in user mode
    Hstatus_hu   = 1 << 9,
    // Raises virtual instruction exception for guest WFI after time limit.
    Hstatus_vtw  = 1 << 21,
  };

  enum : Mword
  {
    Hgatp_mode      = Satp_mode,

    Hgatp_ppn_bits  = Satp_ppn_bits,

    Hgatp_vmid_shift = Satp_asid_shift,
    Hgatp_vmid_bits  = Satp_asid_bits - 2,
  };

  enum : Mword
  {
    Int_virtual_supervisor_software =  2 | Msb,
    Int_virtual_supervisor_timer    =  6 | Msb,
    Int_virtual_supervisor_external = 10 | Msb,
  };

  enum : Mword
  {
    Exc_hcall                  = 10,
    Exc_guest_inst_page_fault  = 20,
    Exc_guest_load_page_fault  = 21,
    Exc_virtual_inst           = 22,
    Exc_guest_store_page_fault = 23,
  };

  enum : Mword
  {
    Hcounteren_cy = 1 << 0,
    Hcounteren_tm = 1 << 1,
  };

  enum : Unsigned64
  {
    Henvcfg_fiom = 1ull << 0,  // Fence of I/O implies Memory
  };

  enum : Mword
  {
    /// Hstatus bits that are controllable from user-mode.
    Hstatus_user_mask =   Hstatus_vsbe | Hstatus_gva | Hstatus_spvp
                        | Hstatus_vsxl | Hstatus_vtw,
    /// Hstatus bits that are set for user mode context by default.
    Hstatus_user_default = 0,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && cpu_virt && fpu && lazy_fpu]:

EXTENSION
class Cpu
{
public:
  enum : Mword
  {
    // Cannot delegate illegal instruction exception as we need it for lazy FPU
    // switching.
    Hedeleg_mask = ~(1UL << Exc_illegal_inst),
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && cpu_virt && fpu && !lazy_fpu]:

EXTENSION
class Cpu
{
public:
  enum : Mword
  {
    Hedeleg_mask = ~0UL,
  };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

#include "panic.h"

IMPLEMENT_OVERRIDE
void
Cpu::init_hypervisor_ext()
{
  if (!has_isa_ext(Isa_ext_h))
    panic("Hypervisor extension not available!");

  // Initialize certain hypervisor extension control registers
  csr_write(hip, 0);
  csr_write(hie, 0);

  // Allow guest read access to cycle and timer csr
  csr_write(hcounteren, Cpu::Hcounteren_cy | Cpu::Hcounteren_tm);

  Unsigned64 envcfg = Henvcfg_fiom;
  csr_write64(henvcfg, envcfg);
}

PUBLIC static inline
Mword
Cpu::get_hgatp()
{
  return csr_read(hgatp);
}

PUBLIC static inline
void
Cpu::set_hgatp_unchecked(Mword vmid, Mword ppn)
{
  csr_write(hgatp, Hgatp_mode | (vmid << Hgatp_vmid_shift) | ppn);
}

PUBLIC static inline NEEDS[<cassert>]
void
Cpu::set_hgatp(Mword vmid, Mword ppn)
{
  assert(ppn < (1UL << Hgatp_vmid_shift));
  assert(vmid == (vmid & ((1 << Hgatp_vmid_bits) - 1)));

  set_hgatp_unchecked(vmid, ppn);
}

PUBLIC static inline
Mword
Cpu::get_vmid()
{
  return cxx::get_lsb(csr_read(hgatp) >> Hgatp_vmid_shift, Hgatp_vmid_bits);
}

PUBLIC static inline
void
Cpu::set_vmid(Mword vmid)
{
  Mword ppn = cxx::get_lsb(get_hgatp(), Hgatp_ppn_bits);
  set_hgatp(vmid, ppn);
}
