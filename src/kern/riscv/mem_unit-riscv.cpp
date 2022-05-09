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
Mem_unit::make_coherent_to_pou(void const *v, size_t)
{
  (void)v;
  local_flush_icache();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && mp]:

#include "cpu.h"
#include "sbi.h"

PUBLIC static inline NEEDS["cpu.h", "sbi.h"]
void
Mem_unit::make_coherent_to_pou(void const *v, size_t)
{
  (void)v;
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
