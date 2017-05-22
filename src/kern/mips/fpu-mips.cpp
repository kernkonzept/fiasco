INTERFACE [mips && fpu]:

#include <cxx/bitfield>

EXTENSION class Fpu
{
public:
  struct Fpu_regs
  {
    Unsigned64 regs[32];
    Mword fcsr;
  };

  struct Fir
  {
    Mword v;

    Fir() = default;
    explicit Fir(Mword v) : v(v) {}

    CXX_BITFIELD_MEMBER(0, 7, revision, v);
    CXX_BITFIELD_MEMBER(8, 15, processor_id, v);
    CXX_BITFIELD_MEMBER(16, 16, s, v);
    CXX_BITFIELD_MEMBER(17, 17, d, v);
    CXX_BITFIELD_MEMBER(18, 18, ps, v);
    //CXX_BITFIELD_MEMBER(19, 19, _3D, v);
    CXX_BITFIELD_MEMBER(20, 20, w, v);
    CXX_BITFIELD_MEMBER(21, 21, l, v);
    CXX_BITFIELD_MEMBER(22, 22, f64, v);
    //CXX_BITFIELD_MEMBER(23, 23, Has2008, v);
    //CXX_BITFIELD_MEMBER(24, 24, FC, v);
    CXX_BITFIELD_MEMBER(28, 28, ufrp, v);
    CXX_BITFIELD_MEMBER(29, 29, frep, v);
  };

  Fir fir() const { return _fir; }

private:
  Fir _fir;
};


// ------------------------------------------------------------------------
IMPLEMENTATION [mips && fpu]:

#include <cassert>
#include <cstdio>
#include <cstring>

#include "fpu_state.h"
#include "kdb_ke.h"
#include "mem.h"
#include "processor.h"
#include "static_assert.h"
#include "cp0_status.h"
#include "cpu.h"

PUBLIC static inline
Mword
Fpu::fir_read()
{
  Mword fir;
  __asm__ __volatile__(
      ".set push    \n"
      ".set reorder \n"
      ".set mips1   \n"
      "cfc1 %0, $0  \n"
      ".set pop     \n"
      : "=r" (fir));
  return fir;
}

PUBLIC static inline
Mword
Fpu::fcr_read()
{
  Mword fcr;
  __asm__ __volatile__(
      ".set push    \n"
      ".set reorder \n"
      ".set mips1   \n"
      "cfc1 %0, $31 \n"
      ".set pop     \n"
      : "=r" (fcr));
  return fcr;
}

PUBLIC static inline NEEDS ["fpu_state.h", <cassert>]
Mword
Fpu::fcr(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());

  assert(fpu_regs);
  return fpu_regs->fcsr;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && fpu && !mips_r6]:

PUBLIC static inline NEEDS ["cp0_status.h"]
bool
Fpu::mode_64bit()
{
  return Cp0_status::read() & Cp0_status::ST_FR;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips32 && fpu && !mips_r6]:

PRIVATE static inline ALWAYS_INLINE
void
Fpu::set_mipsr2_fp64()
{
  asm volatile(".macro set_mipsr2_fp64\n"
               "        .set mips32r2 \n"
               "        .set fp=64    \n"
               ".endm                 \n");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips64 && fpu && !mips_r6]:

PRIVATE static inline ALWAYS_INLINE
void
Fpu::set_mipsr2_fp64()
{
  asm volatile(".macro set_mipsr2_fp64\n"
               "        .set mips64r2 \n"
               "        .set fp=64    \n"
               ".endm                 \n");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && fpu && mips_r6]:

PRIVATE static inline
bool
Fpu::mode_64bit()
{
  return true;
}

PRIVATE static inline ALWAYS_INLINE
void
Fpu::set_mipsr2_fp64()
{
  asm volatile(".macro set_mipsr2_fp64\n"
               ".endm                 \n");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && fpu]:

PRIVATE static
void
Fpu::show(Cpu_number cpu)
{
  const Fir f = fpu.cpu(cpu).fir();

  printf("FPU[%d]: fir:%08x ID:%x Rev:%x fp-type%s%s%s%s%s F64:%x "
         "UFRP:%x FREP:%x\n",
         cxx::int_value<Cpu_number>(cpu),
         (int)f.v,
         (int)f.processor_id(),
         (int)f.revision(),
         f.s() ? ":S" : "",
         f.d() ? ":D" : "",
         f.ps() ? ":PS" : "",
         f.w() ? ":W" : "",
         f.l() ? ":L" : "",
         (int)f.f64(), (int)f.ufrp(), (int)f.frep());
}

IMPLEMENT
void
Fpu::init(Cpu_number cpu, bool resume)
{
  Fpu &f = fpu.cpu(cpu);

  f.enable();
  f._fir = Fir(fir_read());

  if (!resume)
    show(cpu);

  f.disable();
  f.set_owner(0);
}

PRIVATE static inline
void
Fpu::fpu_save_16even(Fpu_regs *r)
{
  Mword tmp;

  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "cfc1   %[tmp],   $31          \n"
               "sw     %[tmp],   %[fcsr]      \n"
               "sdc1   $f0,  (8 *  0)(%[regs])\n"
               "sdc1   $f2,  (8 *  2)(%[regs])\n"
               "sdc1   $f4,  (8 *  4)(%[regs])\n"
               "sdc1   $f6,  (8 *  6)(%[regs])\n"
               "sdc1   $f8,  (8 *  8)(%[regs])\n"
               "sdc1   $f10, (8 * 10)(%[regs])\n"
               "sdc1   $f12, (8 * 12)(%[regs])\n"
               "sdc1   $f14, (8 * 14)(%[regs])\n"
               "sdc1   $f16, (8 * 16)(%[regs])\n"
               "sdc1   $f18, (8 * 18)(%[regs])\n"
               "sdc1   $f20, (8 * 20)(%[regs])\n"
               "sdc1   $f22, (8 * 22)(%[regs])\n"
               "sdc1   $f24, (8 * 24)(%[regs])\n"
               "sdc1   $f26, (8 * 26)(%[regs])\n"
               "sdc1   $f28, (8 * 28)(%[regs])\n"
               "sdc1   $f30, (8 * 30)(%[regs])\n"
               ".set   pop                    \n"
               : [fcsr] "=m"  (r->fcsr),
                 [tmp]  "=&r" (tmp)
               : [regs] "r"   (r->regs));
}

PRIVATE static inline
void
Fpu::fpu_save_16odd(Fpu_regs *r)
{
  set_mipsr2_fp64();
  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "set_mipsr2_fp64               \n"
               ".purgem set_mipsr2_fp64       \n"
               "sdc1   $f1,  (8 *  1)(%[regs])\n"
               "sdc1   $f3,  (8 *  3)(%[regs])\n"
               "sdc1   $f5,  (8 *  5)(%[regs])\n"
               "sdc1   $f7,  (8 *  7)(%[regs])\n"
               "sdc1   $f9,  (8 *  9)(%[regs])\n"
               "sdc1   $f11, (8 * 11)(%[regs])\n"
               "sdc1   $f13, (8 * 13)(%[regs])\n"
               "sdc1   $f15, (8 * 15)(%[regs])\n"
               "sdc1   $f17, (8 * 17)(%[regs])\n"
               "sdc1   $f19, (8 * 19)(%[regs])\n"
               "sdc1   $f21, (8 * 21)(%[regs])\n"
               "sdc1   $f23, (8 * 23)(%[regs])\n"
               "sdc1   $f25, (8 * 25)(%[regs])\n"
               "sdc1   $f27, (8 * 27)(%[regs])\n"
               "sdc1   $f29, (8 * 29)(%[regs])\n"
               "sdc1   $f31, (8 * 31)(%[regs])\n"
               ".set   pop                    \n"
               :: [regs] "r" (r->regs));
}

PRIVATE static inline
void
Fpu::fpu_restore_16even(Fpu_regs *r)
{
  Mword tmp;

  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "ldc1   $f0,  (8 *  0)(%[regs])\n"
               "ldc1   $f2,  (8 *  2)(%[regs])\n"
               "ldc1   $f4,  (8 *  4)(%[regs])\n"
               "ldc1   $f6,  (8 *  6)(%[regs])\n"
               "ldc1   $f8,  (8 *  8)(%[regs])\n"
               "ldc1   $f10, (8 * 10)(%[regs])\n"
               "ldc1   $f12, (8 * 12)(%[regs])\n"
               "ldc1   $f14, (8 * 14)(%[regs])\n"
               "ldc1   $f16, (8 * 16)(%[regs])\n"
               "ldc1   $f18, (8 * 18)(%[regs])\n"
               "ldc1   $f20, (8 * 20)(%[regs])\n"
               "ldc1   $f22, (8 * 22)(%[regs])\n"
               "ldc1   $f24, (8 * 24)(%[regs])\n"
               "ldc1   $f26, (8 * 26)(%[regs])\n"
               "ldc1   $f28, (8 * 28)(%[regs])\n"
               "ldc1   $f30, (8 * 30)(%[regs])\n"
               "lw     %[tmp], %[fcsr]        \n"
               "ctc1   %[tmp], $31            \n"
               ".set   pop                    \n"
               : [tmp]  "=&r" (tmp)
               : [regs] "r"   (r->regs),
                 [fcsr] "m"   (r->fcsr));
}

PRIVATE static inline
void
Fpu::fpu_restore_16odd(Fpu_regs *r)
{
  set_mipsr2_fp64();
  asm volatile(".set   push                   \n"
               ".set   hardfloat              \n"
               "set_mipsr2_fp64               \n"
               ".purgem set_mipsr2_fp64       \n"
               "ldc1   $f1,  (8 *  1)(%[regs])\n"
               "ldc1   $f3,  (8 *  3)(%[regs])\n"
               "ldc1   $f5,  (8 *  5)(%[regs])\n"
               "ldc1   $f7,  (8 *  7)(%[regs])\n"
               "ldc1   $f9,  (8 *  9)(%[regs])\n"
               "ldc1   $f11, (8 * 11)(%[regs])\n"
               "ldc1   $f13, (8 * 13)(%[regs])\n"
               "ldc1   $f15, (8 * 15)(%[regs])\n"
               "ldc1   $f17, (8 * 17)(%[regs])\n"
               "ldc1   $f19, (8 * 19)(%[regs])\n"
               "ldc1   $f21, (8 * 21)(%[regs])\n"
               "ldc1   $f23, (8 * 23)(%[regs])\n"
               "ldc1   $f25, (8 * 25)(%[regs])\n"
               "ldc1   $f27, (8 * 27)(%[regs])\n"
               "ldc1   $f29, (8 * 29)(%[regs])\n"
               "ldc1   $f31, (8 * 31)(%[regs])\n"
               ".set   pop                    \n"
               :: [regs] "r" (r->regs));
}

IMPLEMENT
void
Fpu::save_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());

  assert(fpu_regs);

  if (Fpu::mode_64bit())
    fpu_save_16odd(fpu_regs);

  fpu_save_16even(fpu_regs);
}

IMPLEMENT
void
Fpu::restore_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());

  assert(fpu_regs);

  if (Fpu::mode_64bit())
    fpu_restore_16odd(fpu_regs);

  fpu_restore_16even(fpu_regs);
}

IMPLEMENT inline
unsigned
Fpu::state_size()
{ return sizeof (Fpu_regs); }

IMPLEMENT inline
unsigned
Fpu::state_align()
{ return sizeof(Unsigned64); }

IMPLEMENT
void
Fpu::init_state(Fpu_state *s)
{
  Fpu_regs *fpu_regs = reinterpret_cast<Fpu_regs *>(s->state_buffer());
  static_assert(!(sizeof (*fpu_regs) % sizeof(Mword)),
                "Non-mword size of Fpu_regs");

  // Load the FPU with signalling NANS.  This bit pattern we're using has
  // the property that no matter whether considered as single or as double
  // precision represents signaling NANS.
  Mem::memset_mwords(fpu_regs, -1UL, sizeof (*fpu_regs) / sizeof(Mword));

  // We initialize fcr31 to rounding to nearest, no exceptions.
  fpu_regs->fcsr = 0;
}

PUBLIC static inline NEEDS ["cp0_status.h"]
bool
Fpu::is_enabled()
{
  return Cp0_status::read() & Cp0_status::ST_CU1;
}

PUBLIC static inline NEEDS ["cpu.h", "cp0_status.h"]
void
Fpu::enable()
{
  Cp0_status::set_status_bit(Cp0_status::ST_CU1);
}

PUBLIC static inline NEEDS ["cpu.h", "cp0_status.h"]
void
Fpu::disable()
{
  Cp0_status::clear_status_bit(Cp0_status::ST_CU1);
}

