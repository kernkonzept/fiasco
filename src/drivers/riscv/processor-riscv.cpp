INTERFACE[riscv]:

EXTENSION class Proc
{
public:
  enum
  {
    // We assume that hart masks can be represented by a single word.
    // Hart masks are needed for certain SBI calls (e.g. sbi_send_ipi).
    Max_hart_id = sizeof(Mword) * 8 - 1,
  };

  /**
   * A per hart context, pointed to by the tp register.
   *
   * The RISC-V supervisor mode provides no means to retrieve the ID of the
   * current hart. Therefore, we provide our own means to identify a hart,
   * by having a per hart context data structure.
   */
  struct Hart_context
  {
    // Trap handling routine uses this fields as scratch space.
    Mword _kernel_sp = 0;
    Mword _trap_sp = 0;

    // ID of this hart
    Cpu_phys_id _phys_id;
    // Logical cpu number of this hart
    Cpu_number _cpu_id = Cpu_number::nil();
  };

private:
  enum
  {
    Sstatus_sie = 1 << 1,
  };
};

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

#include "csr.h"

IMPLEMENT static inline NEEDS["csr.h"]
void
Proc::cli()
{
  csr_clear(sstatus, Sstatus_sie);
}

IMPLEMENT static inline NEEDS["csr.h"]
void
Proc::sti()
{
  csr_set(sstatus, Sstatus_sie);
}

IMPLEMENT static inline NEEDS["csr.h"]
Proc::Status
Proc::interrupts()
{
  return csr_read(sstatus) & Sstatus_sie;
}

IMPLEMENT static inline NEEDS["csr.h"]
Proc::Status
Proc::cli_save()
{
  return csr_read_clear(sstatus, Sstatus_sie) & Sstatus_sie;
}

IMPLEMENT static inline NEEDS["csr.h"]
void
Proc::sti_restore(Status status)
{
  csr_set(sstatus, status & Sstatus_sie);
}

IMPLEMENT static inline
void
Proc::pause()
{}

IMPLEMENT static inline
void
Proc::halt()
{
  Status f = cli_save();
  __asm__ __volatile__ ("wfi" : : :  "memory");
  sti_restore(f);
}

IMPLEMENT static inline
void
Proc::irq_chance()
{
  __asm__ __volatile__ ("nop; nop;" : : :  "memory");
}

IMPLEMENT static inline
void
Proc::stack_pointer(Mword sp)
{
  __asm__ __volatile__ ("mv sp, %0" : : "r" (sp));
}

IMPLEMENT static inline
Mword
Proc::stack_pointer()
{
  Mword sp = 0;
  __asm__ __volatile__ ("mv %0, sp" : "=r" (sp));
  return sp;
}

IMPLEMENT static inline
Mword
Proc::program_counter()
{
  Mword pc = 0;
  __asm__ __volatile__ ("auipc %0, 0" : "=r" (pc));
  return pc;
}

PUBLIC static inline
Cpu_phys_id
Proc::cpu_id()
{
  return hart_context()->_phys_id;
}

PUBLIC static inline
Cpu_number
Proc::cpu()
{
  return hart_context()->_cpu_id;
}

PUBLIC static inline
Proc::Hart_context const *
Proc::hart_context()
{
  Mword hc;
  __asm__ __volatile__ ("mv %0, tp" : "=r" (hc));
  return reinterpret_cast<Hart_context *>(hc);
}

PUBLIC static inline
void
Proc::hart_context(Hart_context *hc)
{
  __asm__ __volatile__ ("mv tp, %0" : : "r" (hc));
}
