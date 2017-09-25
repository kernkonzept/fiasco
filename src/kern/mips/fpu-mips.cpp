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

PRIVATE static inline
void
Fpu::set_mips32r2_fp64()
{
  asm volatile(".set   mips32r2\n");
  asm volatile(".set   fp=64\n");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips64 && fpu && !mips_r6]:

PRIVATE static inline
void
Fpu::set_mips32r2_fp64()
{
  asm volatile(".set   mips64r2\n");
  asm volatile(".set   fp=64\n");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [mips && fpu && mips_r6]:

PRIVATE static inline
bool
Fpu::mode_64bit()
{
  return true;
}

PRIVATE static inline
void
Fpu::set_mips32r2_fp64()
{}

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

  asm volatile(".set   push\n");
  asm volatile(".set   hardfloat\n");
  asm volatile("cfc1   %0,   $31\n" : "=r" (tmp));
  asm volatile("sw     %1,   %0 \n" : "=m" (r->fcsr) : "r" (tmp));
  asm volatile("sdc1   $f0,  %0 \n" : "=m" (r->regs[0]));
  asm volatile("sdc1   $f2,  %0 \n" : "=m" (r->regs[2]));
  asm volatile("sdc1   $f4,  %0 \n" : "=m" (r->regs[4]));
  asm volatile("sdc1   $f6,  %0 \n" : "=m" (r->regs[6]));
  asm volatile("sdc1   $f8,  %0 \n" : "=m" (r->regs[8]));
  asm volatile("sdc1   $f10, %0 \n" : "=m" (r->regs[10]));
  asm volatile("sdc1   $f12, %0 \n" : "=m" (r->regs[12]));
  asm volatile("sdc1   $f14, %0 \n" : "=m" (r->regs[14]));
  asm volatile("sdc1   $f16, %0 \n" : "=m" (r->regs[16]));
  asm volatile("sdc1   $f18, %0 \n" : "=m" (r->regs[18]));
  asm volatile("sdc1   $f20, %0 \n" : "=m" (r->regs[20]));
  asm volatile("sdc1   $f22, %0 \n" : "=m" (r->regs[22]));
  asm volatile("sdc1   $f24, %0 \n" : "=m" (r->regs[24]));
  asm volatile("sdc1   $f26, %0 \n" : "=m" (r->regs[26]));
  asm volatile("sdc1   $f28, %0 \n" : "=m" (r->regs[28]));
  asm volatile("sdc1   $f30, %0 \n" : "=m" (r->regs[30]));
  asm volatile(".set   pop\n");
}

PRIVATE static inline
void
Fpu::fpu_save_16odd(Fpu_regs *r)
{
  asm volatile(".set   push\n");
  asm volatile(".set   hardfloat\n");
  Fpu::set_mips32r2_fp64();
  asm volatile("sdc1   $f1,  %0 \n" : "=m" (r->regs[1]));
  asm volatile("sdc1   $f3,  %0 \n" : "=m" (r->regs[3]));
  asm volatile("sdc1   $f5,  %0 \n" : "=m" (r->regs[5]));
  asm volatile("sdc1   $f7,  %0 \n" : "=m" (r->regs[7]));
  asm volatile("sdc1   $f9,  %0 \n" : "=m" (r->regs[9]));
  asm volatile("sdc1   $f11, %0 \n" : "=m" (r->regs[11]));
  asm volatile("sdc1   $f13, %0 \n" : "=m" (r->regs[13]));
  asm volatile("sdc1   $f15, %0 \n" : "=m" (r->regs[15]));
  asm volatile("sdc1   $f17, %0 \n" : "=m" (r->regs[17]));
  asm volatile("sdc1   $f19, %0 \n" : "=m" (r->regs[19]));
  asm volatile("sdc1   $f21, %0 \n" : "=m" (r->regs[21]));
  asm volatile("sdc1   $f23, %0 \n" : "=m" (r->regs[23]));
  asm volatile("sdc1   $f25, %0 \n" : "=m" (r->regs[25]));
  asm volatile("sdc1   $f27, %0 \n" : "=m" (r->regs[27]));
  asm volatile("sdc1   $f29, %0 \n" : "=m" (r->regs[29]));
  asm volatile("sdc1   $f31, %0 \n" : "=m" (r->regs[31]));
  asm volatile(".set   pop\n");
}

PRIVATE static inline
void
Fpu::fpu_restore_16even(Fpu_regs *r)
{
  Mword tmp;

  asm volatile(".set   push\n");
  asm volatile(".set   hardfloat\n");
  asm volatile("ldc1   $f0,  %0 \n" :: "m" (r->regs[0]));
  asm volatile("ldc1   $f2,  %0 \n" :: "m" (r->regs[2]));
  asm volatile("ldc1   $f4,  %0 \n" :: "m" (r->regs[4]));
  asm volatile("ldc1   $f6,  %0 \n" :: "m" (r->regs[6]));
  asm volatile("ldc1   $f8,  %0 \n" :: "m" (r->regs[8]));
  asm volatile("ldc1   $f10, %0 \n" :: "m" (r->regs[10]));
  asm volatile("ldc1   $f12, %0 \n" :: "m" (r->regs[12]));
  asm volatile("ldc1   $f14, %0 \n" :: "m" (r->regs[14]));
  asm volatile("ldc1   $f16, %0 \n" :: "m" (r->regs[16]));
  asm volatile("ldc1   $f18, %0 \n" :: "m" (r->regs[18]));
  asm volatile("ldc1   $f20, %0 \n" :: "m" (r->regs[20]));
  asm volatile("ldc1   $f22, %0 \n" :: "m" (r->regs[22]));
  asm volatile("ldc1   $f24, %0 \n" :: "m" (r->regs[24]));
  asm volatile("ldc1   $f26, %0 \n" :: "m" (r->regs[26]));
  asm volatile("ldc1   $f28, %0 \n" :: "m" (r->regs[28]));
  asm volatile("ldc1   $f30, %0 \n" :: "m" (r->regs[30]));
  asm volatile("lw     %0,   %1 \n" : "=r" (tmp) : "m" (r->fcsr));
  asm volatile("ctc1   %0,   $31\n" :: "r" (tmp));
  asm volatile(".set   pop\n");
}

PRIVATE static inline
void
Fpu::fpu_restore_16odd(Fpu_regs *r)
{
  asm volatile(".set   push\n");
  asm volatile(".set   hardfloat\n");
  Fpu::set_mips32r2_fp64();
  asm volatile("ldc1   $f1,  %0 \n" :: "m" (r->regs[1]));
  asm volatile("ldc1   $f3,  %0 \n" :: "m" (r->regs[3]));
  asm volatile("ldc1   $f5,  %0 \n" :: "m" (r->regs[5]));
  asm volatile("ldc1   $f7,  %0 \n" :: "m" (r->regs[7]));
  asm volatile("ldc1   $f9,  %0 \n" :: "m" (r->regs[9]));
  asm volatile("ldc1   $f11, %0 \n" :: "m" (r->regs[11]));
  asm volatile("ldc1   $f13, %0 \n" :: "m" (r->regs[13]));
  asm volatile("ldc1   $f15, %0 \n" :: "m" (r->regs[15]));
  asm volatile("ldc1   $f17, %0 \n" :: "m" (r->regs[17]));
  asm volatile("ldc1   $f19, %0 \n" :: "m" (r->regs[19]));
  asm volatile("ldc1   $f21, %0 \n" :: "m" (r->regs[21]));
  asm volatile("ldc1   $f23, %0 \n" :: "m" (r->regs[23]));
  asm volatile("ldc1   $f25, %0 \n" :: "m" (r->regs[25]));
  asm volatile("ldc1   $f27, %0 \n" :: "m" (r->regs[27]));
  asm volatile("ldc1   $f29, %0 \n" :: "m" (r->regs[29]));
  asm volatile("ldc1   $f31, %0 \n" :: "m" (r->regs[31]));
  asm volatile(".set   pop\n");
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

