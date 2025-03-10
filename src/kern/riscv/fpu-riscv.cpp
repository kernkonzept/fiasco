INTERFACE [riscv && fpu && riscv_fpu_single]:

EXTENSION class Fpu
{
public:
  struct Fpu_regs
  {
    Unsigned32 regs[32];
    Mword fcsr;
  };
};

// ------------------------------------------------------------------------
INTERFACE [riscv && fpu && riscv_fpu_double]:

EXTENSION class Fpu
{
public:
  struct Fpu_regs
  {
    Unsigned64 regs[32];
    Mword fcsr;
  };
};

// ------------------------------------------------------------------------
INTERFACE [riscv && fpu]:

class Fpu_state : public Fpu::Fpu_regs {};

// ------------------------------------------------------------------------
IMPLEMENTATION [riscv && fpu]:

#include "asm_riscv.h"
#include "cpu.h"
#include "mem.h"

IMPLEMENT inline NEEDS["mem.h"]
void
Fpu::init_state(Fpu_state *fpu_regs)
{
  static_assert(!(sizeof(*fpu_regs) % sizeof(Mword)),
                "Non-mword size of Fpu_regs");
  Mem::memset_mwords(fpu_regs, 0, sizeof(*fpu_regs) / sizeof(Mword));
}

IMPLEMENT inline
unsigned
Fpu::state_size()
{ return sizeof(Fpu_regs); }

IMPLEMENT inline
unsigned
Fpu::state_align()
{ return 16; }

IMPLEMENT
void
Fpu::init(Cpu_number cpu, bool resume)
{
  Fpu &f = fpu.cpu(cpu);
  if (!resume)
    show(cpu);

  f.finish_init();
}

PRIVATE static inline
void
Fpu::save_fpu_regs(Fpu_regs *r)
{
  Mword fcsr;
  __asm__ __volatile__ (
    "frcsr    %[fcsr]                 \n"
    FREG_S "  f0, (%[fsz] *  0)(%[r]) \n"
    FREG_S "  f1, (%[fsz] *  1)(%[r]) \n"
    FREG_S "  f2, (%[fsz] *  2)(%[r]) \n"
    FREG_S "  f3, (%[fsz] *  3)(%[r]) \n"
    FREG_S "  f4, (%[fsz] *  4)(%[r]) \n"
    FREG_S "  f5, (%[fsz] *  5)(%[r]) \n"
    FREG_S "  f6, (%[fsz] *  6)(%[r]) \n"
    FREG_S "  f7, (%[fsz] *  7)(%[r]) \n"
    FREG_S "  f8, (%[fsz] *  8)(%[r]) \n"
    FREG_S "  f9, (%[fsz] *  9)(%[r]) \n"
    FREG_S " f10, (%[fsz] * 10)(%[r]) \n"
    FREG_S " f11, (%[fsz] * 11)(%[r]) \n"
    FREG_S " f12, (%[fsz] * 12)(%[r]) \n"
    FREG_S " f13, (%[fsz] * 13)(%[r]) \n"
    FREG_S " f14, (%[fsz] * 14)(%[r]) \n"
    FREG_S " f15, (%[fsz] * 15)(%[r]) \n"
    FREG_S " f16, (%[fsz] * 16)(%[r]) \n"
    FREG_S " f17, (%[fsz] * 17)(%[r]) \n"
    FREG_S " f18, (%[fsz] * 18)(%[r]) \n"
    FREG_S " f19, (%[fsz] * 19)(%[r]) \n"
    FREG_S " f20, (%[fsz] * 20)(%[r]) \n"
    FREG_S " f21, (%[fsz] * 21)(%[r]) \n"
    FREG_S " f22, (%[fsz] * 22)(%[r]) \n"
    FREG_S " f23, (%[fsz] * 23)(%[r]) \n"
    FREG_S " f24, (%[fsz] * 24)(%[r]) \n"
    FREG_S " f25, (%[fsz] * 25)(%[r]) \n"
    FREG_S " f26, (%[fsz] * 26)(%[r]) \n"
    FREG_S " f27, (%[fsz] * 27)(%[r]) \n"
    FREG_S " f28, (%[fsz] * 28)(%[r]) \n"
    FREG_S " f29, (%[fsz] * 29)(%[r]) \n"
    FREG_S " f30, (%[fsz] * 30)(%[r]) \n"
    FREG_S " f31, (%[fsz] * 31)(%[r]) \n"
    : [fcsr] "=&r" (fcsr),
      "=m" (r->regs)
    : [r] "r" (r->regs),
      [fsz] "n" (sizeof(r->regs[0])));
  r->fcsr = fcsr;
}

PRIVATE static inline
void
Fpu::restore_fpu_regs(Fpu_regs const *r)
{
  __asm__ __volatile__ (
    FREG_L "  f0, (%[fsz] *  0)(%[r]) \n"
    FREG_L "  f1, (%[fsz] *  1)(%[r]) \n"
    FREG_L "  f2, (%[fsz] *  2)(%[r]) \n"
    FREG_L "  f3, (%[fsz] *  3)(%[r]) \n"
    FREG_L "  f4, (%[fsz] *  4)(%[r]) \n"
    FREG_L "  f5, (%[fsz] *  5)(%[r]) \n"
    FREG_L "  f6, (%[fsz] *  6)(%[r]) \n"
    FREG_L "  f7, (%[fsz] *  7)(%[r]) \n"
    FREG_L "  f8, (%[fsz] *  8)(%[r]) \n"
    FREG_L "  f9, (%[fsz] *  9)(%[r]) \n"
    FREG_L " f10, (%[fsz] * 10)(%[r]) \n"
    FREG_L " f11, (%[fsz] * 11)(%[r]) \n"
    FREG_L " f12, (%[fsz] * 12)(%[r]) \n"
    FREG_L " f13, (%[fsz] * 13)(%[r]) \n"
    FREG_L " f14, (%[fsz] * 14)(%[r]) \n"
    FREG_L " f15, (%[fsz] * 15)(%[r]) \n"
    FREG_L " f16, (%[fsz] * 16)(%[r]) \n"
    FREG_L " f17, (%[fsz] * 17)(%[r]) \n"
    FREG_L " f18, (%[fsz] * 18)(%[r]) \n"
    FREG_L " f19, (%[fsz] * 19)(%[r]) \n"
    FREG_L " f20, (%[fsz] * 20)(%[r]) \n"
    FREG_L " f21, (%[fsz] * 21)(%[r]) \n"
    FREG_L " f22, (%[fsz] * 22)(%[r]) \n"
    FREG_L " f23, (%[fsz] * 23)(%[r]) \n"
    FREG_L " f24, (%[fsz] * 24)(%[r]) \n"
    FREG_L " f25, (%[fsz] * 25)(%[r]) \n"
    FREG_L " f26, (%[fsz] * 26)(%[r]) \n"
    FREG_L " f27, (%[fsz] * 27)(%[r]) \n"
    FREG_L " f28, (%[fsz] * 28)(%[r]) \n"
    FREG_L " f29, (%[fsz] * 29)(%[r]) \n"
    FREG_L " f30, (%[fsz] * 30)(%[r]) \n"
    FREG_L " f31, (%[fsz] * 31)(%[r]) \n"
    "fscsr   %[fcsr]                  \n"
    :
    : [fcsr] "r" (r->fcsr),
      [r] "r" (r->regs),
      "m" (r->regs),
      [fsz] "n" (sizeof(r->regs[0])));
}

IMPLEMENT inline NEEDS[Fpu::save_fpu_regs]
void
Fpu::save_state(Fpu_state *fpu_regs)
{
  save_fpu_regs(fpu_regs);
}

IMPLEMENT inline NEEDS[Fpu::restore_fpu_regs]
void
Fpu::restore_state(Fpu_state const *fpu_regs)
{
  restore_fpu_regs(fpu_regs);
}

PUBLIC static inline NEEDS["cpu.h"]
bool
Fpu::is_enabled()
{
  return (csr_read(sstatus) & Cpu::Fs_mask) != Cpu::Fs_off;
}

PUBLIC static inline NEEDS["cpu.h"]
void
Fpu::disable()
{
  // Cpu::Fs_off is zero, so no set necessary
  csr_clear(sstatus, Cpu::Fs_mask);
}

PUBLIC static inline NEEDS["cpu.h"]
void
Fpu::enable()
{
  csr_clear(sstatus, Cpu::Fs_mask);
  csr_set(sstatus, Cpu::Fs_clean);
}

PRIVATE static
void
Fpu::show(Cpu_number)
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [riscv && fpu && lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{
  disable();
  set_owner(nullptr);
}

//-------------------------------------------------------------------------
IMPLEMENTATION [riscv && fpu && !lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{
  enable();
}
