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
  static bool save_32r;
};

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
IMPLEMENTATION [arm && !fpu]:

#include "trap_state.h"

PUBLIC static inline NEEDS["trap_state.h"]
void
Fpu::save_user_exception_state(bool, Fpu_state *, Trap_state *, Exception_state_user *)
{}

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

#include "fpu_state.h"
#include "mem.h"
#include "processor.h"
#include "static_assert.h"
#include "trap_state.h"

bool Fpu::save_32r;

PUBLIC static inline
Mword
Fpu::fpsid_read()
{
  Mword v;
  asm volatile("mrc p10, 7, %0, cr0, cr0" : "=r" (v));
  return v;
}

PUBLIC static inline
Mword
Fpu::mvfr0()
{
  Mword v;
  asm volatile("mrc p10, 7, %0, cr7, cr0" : "=r" (v));
  return v;
}

PUBLIC static inline
Mword
Fpu::mvfr1()
{
  Mword v;
  asm volatile("mrc p10, 7, %0, cr6, cr0" : "=r" (v));
  return v;
}


PRIVATE static inline
void
Fpu::fpexc(Mword v)
{
  asm volatile("mcr p10, 7, %0, cr8, cr0" : : "r" (v));
}

PUBLIC static inline
Mword
Fpu::fpexc()
{
  Mword v;
  asm volatile("mrc p10, 7, %0, cr8, cr0" : "=r" (v));
  return v;
}

PUBLIC static inline
Mword
Fpu::fpinst()
{
  Mword i;
  asm volatile("mcr p10, 7, %0, cr9,  cr0" : "=r" (i));
  return i;
}

PUBLIC static inline
void
Fpu::fpinst(Mword v)
{
  asm volatile("mrc p10, 7, %0, cr9,  cr0" : : "r" (v));
}

PUBLIC static inline
Mword
Fpu::fpinst2()
{
  Mword i;
  asm volatile("mcr p10, 7, %0, cr10,  cr0" : "=r" (i));
  return i;
}

PUBLIC static inline
void
Fpu::fpinst2(Mword v)
{
  asm volatile("mrc p10, 7, %0, cr10,  cr0" : : "r" (v));
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
      assert(0);
    }

  if (ts->psr & Proc::Status_thumb)
    ts->pc += 2;

  return true;
}

IMPLEMENT
void
Fpu::init(Cpu_number cpu, bool resume)
{
  if (Config::Jdb && !resume && cpu == Cpu_number::boot_cpu())
    printf("FPU: Initialize\n");

  copro_enable();

  Fpu &f = fpu.cpu(cpu);
  f._fpsid = Fpsid(fpsid_read());
  if (cpu == Cpu_number::boot_cpu() && f._fpsid.arch_version() > 1)
    save_32r = (mvfr0() & 0xf) == 2;

  if (!resume)
    show(cpu);

  f.disable();
  f.set_owner(0);
}

PRIVATE static inline
void
Fpu::save_fpu_regs(Fpu_regs *r)
{
  Mword tmp;
  asm volatile("stc p11, cr0, [%0], #128     \n"
               "cmp    %2, #0                \n"
               "stcnel p11, cr0, [%0], #128  \n"
               : "=r" (tmp) : "0" (r->state), "r" (save_32r));
}

PRIVATE static inline
void
Fpu::restore_fpu_regs(Fpu_regs *r)
{
  Mword tmp;
  asm volatile("ldc    p11, cr0, [%0], #128 \n"
               "cmp    %2, #0               \n"
               "ldcnel p11, cr0, [%0], #128 \n"
               : "=r" (tmp) : "0" (r->state), "r" (save_32r));
}

IMPLEMENT
void
Fpu::save_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
  Mword tmp;

  assert(fpu_regs);

  asm volatile ("mrc p10, 7, %[fpexc], cr8,  cr0, 0  \n"
                "orr %[tmp], %[fpexc], #0x40000000   \n"
                "mcr p10, 7, %[tmp], cr8,  cr0, 0    \n" // enable FPU to store full state
                "mrc p10, 7, %[fpscr], cr1,  cr0, 0   \n"
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
Fpu::restore_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());

  assert(fpu_regs);

  restore_fpu_regs(fpu_regs);

  asm volatile ("mcr p10, 7, %0, cr8,  cr0, 0  \n"
                "mcr p10, 7, %1, cr1,  cr0, 0  \n"
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
Fpu::save_user_exception_state(bool owner, Fpu_state *s, Trap_state *ts, Exception_state_user *esu)
{
  if (!(ts->esr.ec() == 7 && ts->esr.cpt_cpnr() == 10))
    return;

  if (owner)
    {
      if (Proc::Is_hyp && !is_enabled())
        fpu.current().enable();

      Mword exc = Fpu::fpexc();

      esu->fpexc = exc;
      if (exc & FPEXC_EX)
        {
          esu->fpinst  = Fpu::fpinst();
          if (exc & FPEXC_FP2V)
            esu->fpinst2 = Fpu::fpinst2();

          if (!Proc::Is_hyp)
            Fpu::fpexc(exc & ~FPEXC_EX);
        }
      return;
    }

  if (!s->state_buffer())
    {
      esu->fpexc = 0;
      return;
    }

  assert (s->state_buffer());

  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
  esu->fpexc = fpu_regs->fpexc;
  if (fpu_regs->fpexc & FPEXC_EX)
    {
      esu->fpinst  = fpu_regs->fpinst;
      if (fpu_regs->fpexc & FPEXC_FP2V)
        esu->fpinst2 = fpu_regs->fpinst2;

      if (!Proc::Is_hyp)
        fpu_regs->fpexc &= ~FPEXC_EX;
    }
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && debug]:

PRIVATE static
void
Fpu::show(Cpu_number cpu)
{
  const Fpsid s = fpu.cpu(cpu)._fpsid;
  printf("FPU%d: Subarch: %x, Part: %x, Rev: %x, Var: %x, Impl: %x\n",
         cxx::int_value<Cpu_number>(cpu),
         (int)s.sub_arch(), (int)s.part_number(),
         (int)s.rev(), (int)s.variant(), (int)s.implementer());
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !debug]:

PRIVATE static inline
void
Fpu::show(Cpu_number)
{}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !cpu_virt]:

IMPLEMENT inline NEEDS ["fpu_state.h", "mem.h", "static_assert.h", <cstring>]
void
Fpu::init_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
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
}

PUBLIC static inline
void
Fpu::disable()
{
  fpexc(fpexc() & ~FPEXC_EN);
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && cpu_virt]:

EXTENSION class Fpu
{
private:
  Mword _fpexc;
};

IMPLEMENT inline NEEDS ["fpu_state.h", "mem.h", "static_assert.h", <cstring>]
void
Fpu::init_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
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
}


IMPLEMENT_OVERRIDE
void
Fpu::restore_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());

  assert(fpu_regs);

  Mword dummy;
  asm volatile ("mcr p10, 7, %[fpexc], cr8,  cr0, 0   \n"
                "mcr p10, 7, %[fpscr], cr1,  cr0, 0   \n"
                : [tmp] "=&r" (dummy)
                : [fpexc] "r" (fpu_regs->fpexc | FPEXC_EN),
                  [fpscr] "r" (fpu_regs->fpscr));

  if (fpu_regs->fpexc & FPEXC_EX)
    {
      fpinst(fpu_regs->fpinst);
      if (fpu_regs->fpexc & FPEXC_FP2V)
        fpinst2(fpu_regs->fpinst2);
    }

  restore_fpu_regs(fpu_regs);

  asm volatile ("mcr p10, 7, %0, cr8,  cr0, 0  \n"
                :
                : "r" (fpu_regs->fpexc));
}
