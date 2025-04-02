INTERFACE [arm && fpu]:

#include <cxx/bitfield>

EXTENSION class Fpu
{
public:
  struct Exception_state_user
  {
    Mword fpexc;
    Mword fpinst;
    Mword fpinst2;
  };

  struct Fpu_regs
  {
    Mword fpexc, fpscr, fpinst, fpinst2;
    Mword state[64];
  };

  enum
  {
    FPEXC_FP2V = 1 << 28,
    FPEXC_EN   = 1 << 30,
    FPEXC_EX   = 1 << 31,
  };

  struct Fpsid
  {
    Mword v;

    Fpsid() = default;
    explicit Fpsid(Mword v) : v(v) {}

    CXX_BITFIELD_MEMBER(0, 3, rev, v);
    CXX_BITFIELD_MEMBER(4, 7, variant, v);
    CXX_BITFIELD_MEMBER(8, 15, part_number, v);
    CXX_BITFIELD_MEMBER(16, 22, sub_arch, v);
    CXX_BITFIELD_MEMBER(16, 19, arch_version, v);
    CXX_BITFIELD_MEMBER(24, 31, implementer, v);
  };

  Fpsid fpsid() const { return _fpsid; }

private:
  Fpsid _fpsid;
  static Global_data<bool> save_32r;
};

class Fpu_state : public Fpu::Fpu_regs {};

// ------------------------------------------------------------------------
INTERFACE [arm && !fpu]:

EXTENSION class Fpu
{
public:
  struct Exception_state_user
  {
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !arm_v6plus]:

PRIVATE static inline
void
Fpu::copro_enable()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && arm_v6plus && !cpu_virt]:

PRIVATE static inline
void
Fpu::copro_enable()
{
  Mword r;
  asm volatile("mrc  p15, 0, %0, c1, c0, 2\n"
               "orr  %0, %0, %1           \n"
               "mcr  p15, 0, %0, c1, c0, 2\n"
               : "=r" (r) : "I" (0x00f00000));
  Mem::isb();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && arm_v6plus && cpu_virt]:

PRIVATE static inline
void
Fpu::copro_enable()
{
  Mword r;
  asm volatile("mrc  p15, 0, %0, c1, c0, 2\n"
               "orr  %0, %0, %1           \n"
               "mcr  p15, 0, %0, c1, c0, 2\n"
               : "=r" (r) : "I" (0x00f00000));
  Mem::isb();
  fpexc((fpexc() | FPEXC_EN)); // & ~FPEXC_EX);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu]:

#include <cassert>
#include <cstdio>
#include <cstring>

#include "mem.h"
#include "processor.h"
#include "trap_state.h"
#include "global_data.h"

DEFINE_GLOBAL Global_data<bool> Fpu::save_32r;

PUBLIC static inline
Mword
Fpu::fpsid_read()
{
  Mword v;
  asm volatile(".fpu vfp\n vmrs %0, fpsid" : "=r" (v));
  return v;
}

PUBLIC static inline
Mword
Fpu::mvfr0()
{
  Mword v;
  asm volatile(".fpu vfp\n vmrs %0, mvfr0" : "=r" (v));
  return v;
}

PUBLIC static inline
Mword
Fpu::mvfr1()
{
  Mword v;
  asm volatile(".fpu vfp\n vmrs %0, mvfr1" : "=r" (v));
  return v;
}


PRIVATE static inline
void
Fpu::fpexc(Mword v)
{
  asm volatile(".fpu vfp\n vmsr fpexc, %0" : : "r" (v));
}

PUBLIC static inline
Mword
Fpu::fpexc()
{
  Mword v;
  asm volatile(".fpu vfp\n vmrs %0, fpexc" : "=r" (v));
  return v;
}

PUBLIC static inline
Mword
Fpu::fpinst()
{
  Mword i;
  asm volatile(".fpu vfp\n vmrs %0, fpinst" : "=r" (i));
  return i;
}

PUBLIC static inline
void
Fpu::fpinst(Mword v)
{
  asm volatile(".fpu vfp\n vmsr fpinst, %0" : : "r" (v));
}

PUBLIC static inline
Mword
Fpu::fpinst2()
{
  Mword i;
  asm volatile(".fpu vfp\n vmrs %0, fpinst2" : "=r" (i));
  return i;
}

PUBLIC static inline
void
Fpu::fpinst2(Mword v)
{
  asm volatile(".fpu vfp\n vmsr fpinst2, %0" : : "r" (v));
}

PUBLIC static inline NEEDS[Fpu::fpexc]
bool
Fpu::exc_pending()
{
  return fpexc() & FPEXC_EX;
}


PUBLIC static inline
int
Fpu::is_emu_insn(Mword opcode)
{
  if ((opcode & 0x0ff00f90) != 0x0ef00a10)
    return false;

  unsigned reg = (opcode >> 16) & 0xf;
  return reg == 0 || reg == 6 || reg == 7;
}

PUBLIC static inline NEEDS[<cassert>]
bool
Fpu::emulate_insns(Mword opcode, Trap_state *ts)
{
  unsigned reg = (opcode >> 16) & 0xf;
  unsigned rt  = (opcode >> 12) & 0xf;
  Fpsid fpsid = Fpu::fpu.current().fpsid();
  switch (reg)
    {
    case 0: // FPSID
      ts->r[rt] = fpsid.v;
      break;
    case 6: // MVFR1
      if (fpsid.arch_version() < 2)
        return false;
      ts->r[rt] = Fpu::mvfr1();
      break;
    case 7: // MVFR0
      if (fpsid.arch_version() < 2)
        return false;
      ts->r[rt] = Fpu::mvfr0();
      break;
    default:
      return false;
    }

  // FPU insns are 32bit, even for thumb
  ts->pc += 4;

  return true;
}

IMPLEMENT
void
Fpu::init(Cpu_number cpu, bool resume)
{
  if constexpr (Config::Jdb)
    {
      if (!resume && cpu == Cpu_number::boot_cpu())
        printf("FPU: Initialize\n");
    }

  copro_enable();

  Fpu &f = fpu.cpu(cpu);
  f._fpsid = Fpsid(fpsid_read());
  if (cpu == Cpu_number::boot_cpu() && f._fpsid.arch_version() > 1)
    save_32r = (mvfr0() & 0xf) == 2;

  if (!resume)
    show(cpu);

  f.finish_init();
}

PRIVATE static inline
void
Fpu::save_fpu_regs(Fpu_regs *r)
{
  Mword tmp;
  asm volatile(".fpu neon\n"
               "cmp    %2, #0          \n"
               "vstm   %0!, {d0-d15}   \n"
               "beq 1f                 \n"
               "vstm   %0!, {d16-d31}  \n"
               "1:                     \n"
               : "=r" (tmp) : "0" (r->state), "r" (save_32r.unwrap()));
}

PRIVATE static inline
void
Fpu::restore_fpu_regs(Fpu_regs const *r)
{
  Mword tmp;
  asm volatile(".fpu neon\n "
               "cmp    %2, #0        \n"
               "vldm   %0!, {d0-d15}  \n"
               "beq 1f                 \n"
               "vldm   %0!, {d16-d31} \n"
               "1:                     \n"
               : "=r" (tmp) : "0" (r->state), "r" (save_32r.unwrap()));
}

IMPLEMENT
void
Fpu::save_state(Fpu_state *fpu_regs)
{
  Mword tmp;

  assert(fpu_regs);

  asm volatile (".fpu vfp \n"
                "vmrs %[fpexc], fpexc  \n"
                "orr %[tmp], %[fpexc], #0x40000000   \n"
                "vmsr fpexc, %[tmp]    \n" // enable FPU to store full state
                "vmrs %[fpscr], fpscr  \n"
                : [tmp] "=&r" (tmp),
                  [fpexc] "=&r" (fpu_regs->fpexc),
                  [fpscr] "=&r" (fpu_regs->fpscr));

  if (fpu_regs->fpexc & FPEXC_EX)
    {
      fpu_regs->fpinst = fpinst();
      if (fpu_regs->fpexc & FPEXC_FP2V)
        fpu_regs->fpinst2 = fpinst2();
    }

  save_fpu_regs(fpu_regs);
}

IMPLEMENT_DEFAULT
void
Fpu::restore_state(Fpu_state const *fpu_regs)
{
  assert(fpu_regs);

  restore_fpu_regs(fpu_regs);

  asm volatile (".fpu vfp        \n"
                "vmsr fpexc, %0  \n"
                "vmsr fpscr, %1  \n"
                :
                : "r" ((fpu_regs->fpexc | FPEXC_EN) & ~FPEXC_EX),
                  "r" (fpu_regs->fpscr));
}

IMPLEMENT inline
unsigned
Fpu::state_size()
{ return sizeof (Fpu_regs); }

IMPLEMENT inline
unsigned
Fpu::state_align()
{ return 4; }

PUBLIC static inline NEEDS["trap_state.h", <cassert>, Fpu::fpexc]
void
Fpu::save_user_exception_state(bool owner, Fpu_state *fpu_regs,
                               Trap_state *ts, Exception_state_user *esu)
{
  if (!(ts->esr.ec() == 7 && ts->esr.cpt_cpnr() == 10))
    return;

  if (owner)
    {
      if constexpr (Proc::Is_hyp)
        {
          if (!is_enabled())
            fpu.current().enable();
        }

      Mword exc = Fpu::fpexc();

      esu->fpexc = exc;
      if (exc & FPEXC_EX)
        {
          esu->fpinst  = Fpu::fpinst();
          if (exc & FPEXC_FP2V)
            esu->fpinst2 = Fpu::fpinst2();

          if constexpr (!Proc::Is_hyp)
            Fpu::fpexc(exc & ~FPEXC_EX);
        }
      return;
    }

  if (!fpu_regs)
    {
      esu->fpexc = 0;
      return;
    }

  esu->fpexc = fpu_regs->fpexc;
  if (fpu_regs->fpexc & FPEXC_EX)
    {
      esu->fpinst  = fpu_regs->fpinst;
      if (fpu_regs->fpexc & FPEXC_FP2V)
        esu->fpinst2 = fpu_regs->fpinst2;

      if constexpr (!Proc::Is_hyp)
        fpu_regs->fpexc &= ~FPEXC_EX;
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{
  disable();
  set_owner(nullptr);
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && debug]:

PRIVATE static
void
Fpu::show(Cpu_number cpu)
{
  const Fpsid s = fpu.cpu(cpu)._fpsid;
  printf("FPU%d: Subarch: %x, Part: %x, Rev: %x, Var: %x, Impl: %x\n",
         cxx::int_value<Cpu_number>(cpu), s.sub_arch().get(),
         s.part_number().get(), s.rev().get(), s.variant().get(),
         s.implementer().get());
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !debug]:

PRIVATE static inline
void
Fpu::show(Cpu_number)
{}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !cpu_virt]:

IMPLEMENT inline NEEDS ["mem.h", <cstring>]
void
Fpu::init_state(Fpu_state *fpu_regs)
{
  static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                "Non-mword size of Fpu_regs");
  Mem::memset_mwords(fpu_regs, 0, sizeof (*fpu_regs) / sizeof(Mword));
}

PUBLIC static
bool
Fpu::is_enabled()
{
  return fpexc() & FPEXC_EN;
}


PUBLIC static inline
void
Fpu::enable()
{
  fpexc((fpexc() | FPEXC_EN)); // & ~FPEXC_EX);
  Mem::isb();
}

PUBLIC static inline
void
Fpu::disable()
{
  fpexc(fpexc() & ~FPEXC_EN);
  Mem::isb();
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && cpu_virt]:

EXTENSION class Fpu
{
private:
  Mword _fpexc;
};

IMPLEMENT inline NEEDS ["mem.h", <cstring>]
void
Fpu::init_state(Fpu_state *fpu_regs)
{
  static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                "Non-mword size of Fpu_regs");
  Mem::memset_mwords(fpu_regs, 0, sizeof (*fpu_regs) / sizeof(Mword));
  fpu_regs->fpexc |= FPEXC_EN;
}

PUBLIC static
bool
Fpu::is_enabled()
{
  Mword dummy; __asm__ __volatile__ ("mrc p15, 4, %0, c1, c1, 2" : "=r"(dummy));
  return !(dummy & 0xc00);
}


PUBLIC inline
void
Fpu::enable()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrc p15, 4, %0, c1, c1, 2 \n"
      "bic %0, %0, #0xc00        \n"
      "mcr p15, 4, %0, c1, c1, 2 \n" : "=&r" (dummy) );
  fpexc(_fpexc);
  Mem::isb();
}

PUBLIC inline
void
Fpu::disable()
{
  Mword dummy;
  if (!is_enabled())
    {
      if (!(_fpexc & FPEXC_EN))
        {
          enable();
          fpexc(_fpexc | FPEXC_EN);
        }
    }
  else
    {
      _fpexc = fpexc();
      if (!(_fpexc & FPEXC_EN))
        fpexc(_fpexc | FPEXC_EN);
    }
  __asm__ __volatile__ (
      "mrc p15, 4, %0, c1, c1, 2 \n"
      "orr %0, %0, #0xc00        \n"
      "mcr p15, 4, %0, c1, c1, 2 \n" : "=&r" (dummy) );
  Mem::isb();
}


IMPLEMENT_OVERRIDE
void
Fpu::restore_state(Fpu_state const *fpu_regs)
{
  assert(fpu_regs);

  asm volatile (".fpu vfp\n"
                "vmsr fpexc, %[fpexc]  \n"
                "vmsr fpscr, %[fpscr]  \n"
                :
                : [fpexc] "r" (fpu_regs->fpexc | FPEXC_EN),
                  [fpscr] "r" (fpu_regs->fpscr));

  if (fpu_regs->fpexc & FPEXC_EX)
    {
      fpinst(fpu_regs->fpinst);
      if (fpu_regs->fpexc & FPEXC_FP2V)
        fpinst2(fpu_regs->fpinst2);
    }

  restore_fpu_regs(fpu_regs);

  asm volatile (".fpu vfp\n  vmsr fpexc, %0  \n"
                :
                : "r" (fpu_regs->fpexc));
}
