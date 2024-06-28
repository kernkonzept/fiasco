INTERFACE [riscv]:

#include "types.h"

class Mem_unit
{
private:
  static unsigned _asid_num;
};

//----------------------------------------------------------------------------
INTERFACE [riscv && riscv_asid]:

EXTENSION class Mem_unit
{
public:
  enum : Mword
  {
    // ASID 0 is used by the kernel during boot.
    Asid_boot     = 0UL,
    Asid_invalid  = ~0UL,
  };

  enum { Have_asids = 1 };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && !riscv_asid]:

#include "types.h"

EXTENSION class Mem_unit
{
public:
  enum : Mword
  {
    // No ASID support, always use zero.
    Asid_disabled = 0UL,
    Asid_boot     = 0UL,
    Asid_invalid  = ~0UL,
  };

  enum { Have_asids = 0 };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && riscv_vmid]:

EXTENSION class Mem_unit
{
public:
  enum : Mword
  {
    Vmid_disabled = 0UL,
    Vmid_invalid  = ~0UL,
  };

  enum { Have_vmids = 1 };

  static Mword _vmid_num;
};

//----------------------------------------------------------------------------
INTERFACE [riscv && !riscv_vmid]:

EXTENSION class Mem_unit
{
public:
  enum { Have_vmids = 0 };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

unsigned Mem_unit::_asid_num = 0;

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush()
{
  __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

PUBLIC static inline
void
Mem_unit::tlb_flush(Mword asid)
{
  __asm__ __volatile__ ("sfence.vma x0, %0" : : "r"(asid) : "memory");
}

PUBLIC static inline
void
Mem_unit::tlb_flush_page(Address addr)
{
  __asm__ __volatile__ ("sfence.vma %0" : : "r"(addr) : "memory");
}

PUBLIC static inline
void
Mem_unit::tlb_flush_page(Address addr, Mword asid)
{
  __asm__ __volatile__ ("sfence.vma %0, %1" : : "r"(addr), "r"(asid) : "memory");
}

PUBLIC static inline ALWAYS_INLINE
void
Mem_unit::tlb_flush_kernel(Address addr)
{
  return tlb_flush_page(addr);
}

PUBLIC static inline
void
Mem_unit::local_flush_icache()
{
  __asm__ __volatile__ ("fence.i" : : : "memory");
}

PUBLIC static inline
unsigned
Mem_unit::asid_num()
{
  return _asid_num;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !mp]:

PUBLIC static inline
void
Mem_unit::make_coherent_to_pou(void const *, size_t)
{
  local_flush_icache();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

#include "cpu.h"
#include "sbi.h"

PUBLIC static inline NEEDS["cpu.h", "sbi.h"]
void
Mem_unit::make_coherent_to_pou(void const *, size_t)
{
  local_flush_icache();

  Cpu_mask cpus;
  cpus = Cpu::online_mask();
  cpus.atomic_clear(current_cpu());
  Sbi::remote_fence_i(Hart_mask(cpus));
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && riscv_asid]:

#include "cpu.h"
#include "panic.h"

PUBLIC static
void
Mem_unit::init_asids()
{
  Mword prev_asid = Cpu::get_asid();

  // XXX We assume that all Harts implement the same amount of ASIDs.

  // Write one to every ASID bit, then read back the value. The number of
  // implemented ASID bits corresponds to the bits that are still one.
  Cpu::set_asid((1 << Cpu::Satp_asid_bits) - 1);
  Mword max_asid = Cpu::get_asid();

  // Restore previous ASID bits
  if (prev_asid != max_asid)
    {
      Cpu::set_asid(prev_asid);
      tlb_flush(max_asid);
    }

  _asid_num = max_asid ? max_asid + 1 : 0;
  printf("CPU implements %u ASIDs.\n", _asid_num);

  // We need at least as many ASIDs as the maximum possible number of CPUs,
  // otherwise the generation roll over mechanism of our ASID allocator breaks.
  if (_asid_num < Config::Max_num_cpus)
    panic("CPU does not provide enough ASIDs: %d required",
          Config::Max_num_cpus);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !riscv_asid]:

PUBLIC static inline
void
Mem_unit::init_asids()
{
  _asid_num = 0;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && riscv_vmid]:

#include "cpu.h"
#include "panic.h"

Mword Mem_unit::_vmid_num = 0;

PUBLIC static inline
unsigned
Mem_unit::vmid_num()
{
  return _vmid_num;
}

PUBLIC static
void
Mem_unit::init_vmids()
{
  Mword prev_vmid = Cpu::get_vmid();

  // Write one to every VMID bit, then read back the value. The number of
  // implemented VMID bits corresponds to the bits that are still one.
  Cpu::set_vmid((1 << Cpu::Hgatp_vmid_bits) - 1);
  Mword max_vmid = Cpu::get_vmid();

  // Restore previous VMID bits
  if (prev_vmid != max_vmid)
    {
      Cpu::set_vmid(prev_vmid);
      hfence_gvma_vmid(max_vmid);
    }

  _vmid_num = max_vmid ? max_vmid + 1 : 0;
  printf("Cpu implements %lu VMIDs.\n", _vmid_num);

  if (_vmid_num < Config::Max_num_cpus)
    panic("Cpu does not provide enough VMIDs.");
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !riscv_vmid]:

PUBLIC static inline
void
Mem_unit::init_vmids()
{
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && cpu_virt]:

PUBLIC static inline
void
Mem_unit::hfence_gvma()
{
  /*
   * rs1 = zero
   * rs2 = zero
   * HFENCE.GVMA
   * 0110001 00000 00000 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x62000073" : : : "memory" );
}

PUBLIC static inline
void
Mem_unit::hfence_gvma_gpa(Address guest_addr)
{
  register Mword a0 __asm__("a0") = guest_addr;
  /*
   * rs1 = a0 (GPA)
   * rs2 = zero
   * HFENCE.GVMA zero, a0
   * 0110001 00000 01010 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x62050073" : : "r"(a0) : "memory");
}

PUBLIC static inline
void
Mem_unit::hfence_gvma_vmid(Mword vmid)
{
  register Mword a0 __asm__("a0") = vmid;
  /*
   * rs1 = zero
   * rs2 = a0 (VMID)
   * HFENCE.GVMA zero, a0
   * 0110001 01010 00000 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x62a00073" : : "r"(a0) : "memory");
}

PUBLIC static inline
void
Mem_unit::hfence_gvma_gpa_vmid(Mword guest_addr, Mword vmid)
{
  register Mword a0 __asm__("a0") = guest_addr;
  register Mword a1 __asm__("a1") = vmid;
  /*
   * rs1 = a0 (GPA)
   * rs2 = a1 (VMID)
   * HFENCE.GVMA a0, a1
   * 0110001 01011 01010 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x62b50073" : : "r"(a0), "r"(a1) : "memory");
}

PUBLIC static inline
void
Mem_unit::hfence_vvma()
{
  /*
   * rs1 = zero
   * rs2 = zero
   * HFENCE.VVMA
   * 0010001 00000 00000 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x22000073" : : : "memory" );
}

PUBLIC static inline
void
Mem_unit::hfence_vvma_va(Address virt_addr)
{
  register Mword a0 __asm__("a0") = virt_addr;
  /*
   * rs1 = a0 (VA)
   * rs2 = zero
   * HFENCE.VVMA zero, a0
   * 0010001 00000 01010 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x22050073" : : "r"(a0) : "memory");
}

PUBLIC static inline
void
Mem_unit::hfence_vvma_asid(Mword asid)
{
  register Mword a0 __asm__("a0") = asid;
  /*
   * rs1 = zero
   * rs2 = a0 (ASID)
   * HFENCE.VVMA zero, a0
   * 0010001 01010 00000 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x22a00073" : : "r"(a0) : "memory");
}

PUBLIC static inline
void
Mem_unit::hfence_vvma_va_asid(Mword virt_addr, Mword asid)
{
  register Mword a0 __asm__("a0") = virt_addr;
  register Mword a1 __asm__("a1") = asid;
  /*
   * rs1 = a0 (VA)
   * rs2 = a1 (ASID)
   * HFENCE.VVMA a0, a1
   * 0010001 01011 01010 000 00000 1110011
   */
  __asm__ __volatile__ (".word 0x22b50073" : : "r"(a0), "r"(a1) : "memory");
}
