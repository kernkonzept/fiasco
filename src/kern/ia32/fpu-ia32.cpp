INTERFACE[ia32,amd64]:

EXTENSION class Fpu
{

private:
  struct fpu_regs       // saved FPU registers
  {
    long    cwd;
    long    swd;
    long    twd;
    long    fip;
    long    fcs;
    long    foo;
    long    fos;
    long    st_space[20];   /* 8*10 bytes for each FP-reg = 80 bytes */
  };

  struct sse_regs
  {
    Unsigned16 cwd;
    Unsigned16 swd;
    Unsigned16 twd;
    Unsigned16 fop;
    Unsigned32 fip;
    Unsigned32 fcs;
    Unsigned32 foo;
    Unsigned32 fos;
    Unsigned32 mxcsr;
    Unsigned32 reserved;
    Unsigned32 st_space[32];   /*  8*16 bytes for each FP-reg  = 128 bytes */
    Unsigned32 xmm_space[64];  /* 16*16 bytes for each XMM-reg = 256 bytes */
    Unsigned32 padding[24];
  };

  struct Xsave_buffer
  {
    sse_regs sse;
    Unsigned64 header[8];
  };

  enum Variants
  {
    Variant_fpu,
    Variant_fxsr,
    Variant_xsave,
  };

  enum Variants _variant;

  static unsigned _state_size;
  static unsigned _state_align;
};


//----------------------------------------------------------------
IMPLEMENTATION [{ia32,amd64}-fpu]:

EXTENSION class Fpu
{
public:
  Unsigned64 get_xcr0() const
  { return _xcr0; }

private:
  Unsigned64 _xcr0;
};

#include "cpu.h"
#include "globalconfig.h"
#include "globals.h"
#include "regdefs.h"

#include <cassert>
#include <cstdio>

PRIVATE static
void
Fpu::init_xsave(Cpu_number cpu)
{
  Unsigned32 eax, ebx, ecx, edx;

  Cpu::cpus.cpu(cpu).cpuid(0xd, 0, &eax, &ebx, &ecx, &edx);

  // allow safe features: x87, SSE, AVX and AVX512
  Fpu::fpu.cpu(cpu)._xcr0 =
    ((Unsigned64{edx} << 32) | eax) & Cpu::Xstate_defined_bits;

  // enable xsave features
  Cpu::cpus.cpu(cpu).set_cr4(Cpu::cpus.cpu(cpu).get_cr4() | CR4_OSXSAVE);
  Cpu::xsetbv(Fpu::fpu.cpu(cpu).get_xcr0(), 0);
}

PRIVATE static
void
Fpu::init_disable()
{
  // disable Coprocessor Emulation to allow exception #7/NM on TS
  // enable Numeric Error (exception #16/MF, native FPU mode)
  // enable Monitor Coprocessor
  Cpu::set_cr0((Cpu::get_cr0() & ~CR0_EM) | CR0_NE | CR0_MP);
}

/*
 * Save FPU or SSE state
 */
IMPLEMENT inline NEEDS [<cassert>,"globals.h", "regdefs.h", "cpu.h"]
void
Fpu::save_state(Fpu_state *s)
{
  assert (s);

  // Both fxsave and fnsave are non-waiting instructions and thus
  // cannot cause exception #16 for pending FPU exceptions.

  switch (fpu.current()._variant)
    {
    case Variant_xsave:
      asm volatile("xsave (%2)" : : "a" (~0UL), "d" (~0UL), "r" (s) : "memory");
      break;
    case Variant_fxsr:
      asm volatile ("fxsave (%0)" : : "r" (s) : "memory");
      break;
    case Variant_fpu:
      asm volatile ("fnsave (%0)" : : "r" (s) : "memory");
      break;
    }
}

/*
 * Restore a saved FPU or SSE state
 */
IMPLEMENT inline NEEDS ["globals.h", "globalconfig.h", <cassert>,"regdefs.h",
                        "cpu.h"]
void
Fpu::restore_state(Fpu_state const *s)
{
  assert (s);

  // Only fxrstor is a non-waiting instruction and thus
  // cannot cause exception #16 for pending FPU exceptions.
  Fpu &f = fpu.current();

  switch (f._variant)
    {
    case Variant_xsave:
      asm volatile ("xrstor (%2)" : : "a" (~0UL), "d" (~0UL), "r" (s));
      break;
    case Variant_fxsr:
        {
#if !defined (CONFIG_WORKAROUND_AMD_FPU_LEAK)
          asm volatile ("fxrstor (%0)" : : "r" (s));
#else
          /* The code below fixes a security leak on AMD CPUs, where
           * some registers of the FPU are not restored from the state_buffer
           * if there are no FPU exceptions pending. The old values, from the
           * last FPU owner, are therefore leaked to the new FPU owner.
           */
          static Mword int_dummy = 0;

          asm volatile(
              "fnstsw	%%ax    \n\t"   // save fpu flags in ax
              "ffree	%%st(7) \n\t"   // make enough space for the fildl
              "bt       $7,%%ax \n\t"   // test if exception bit is set
              "jnc      1f      \n\t"
              "fnclex           \n\t"   // clear it
              "1: fildl %1      \n\t"   // dummy load which sets the
              // affected to def. values
              "fxrstor (%0)     \n\t"   // finally restore the state
              : : "r" (s), "m" (int_dummy) : "ax");
#endif
        }
      break;
    case Variant_fpu:
#ifdef CONFIG_LAZY_FPU
      // this should be handled in the cases where we release the FPU and it has no owner anymore...

      // frstor is a waiting instruction and we must make sure no
      // FPU exceptions are pending here. We distinguish two cases:
      // 1) If we had a previous FPU owner, we called save_state before and
      //    invoked fnsave which re-initialized the FPU and cleared exceptions
      // 2) Otherwise we call fnclex instead to clear exceptions.

      if (!f.owner())
        asm volatile ("fnclex");
#endif

      asm volatile ("frstor (%0)" : : "r" (s));
      break;
    }
}

/*
 * Mark the FPU busy. The next attempt to use it will yield a trap.
 */
PUBLIC static inline NEEDS ["regdefs.h","cpu.h"]
void
Fpu::disable()
{
  Cpu::set_cr0(Cpu::get_cr0() | CR0_TS);
}

/*
 * Mark the FPU no longer busy. Subsequent FPU access won't trap.
 */
PUBLIC static inline
void
Fpu::enable()
{
  asm volatile ("clts");
}

//----------------------------------------------------------------
IMPLEMENTATION[ia32,amd64]:

#include <cstring>
#include "cpu.h"
#include "regdefs.h"
#include "globals.h"

unsigned Fpu::_state_size;
unsigned Fpu::_state_align;

/*
 * Initialize FPU or SSE state
 * We don't use finit, because it is slow. Initializing the context in
 * memory and fetching it via restore_state is supposedly faster
 */
IMPLEMENT inline NEEDS ["cpu.h", "globals.h", "regdefs.h", <cstring>]
void
Fpu::init_state(Fpu_state *s)
{
  Cpu const &_cpu = *Cpu::boot_cpu();
  if (_cpu.features() & FEAT_FXSR)
    {
      assert (_state_size >= sizeof (sse_regs));
      sse_regs *sse = reinterpret_cast<sse_regs *>(s);

      memset(sse, 0, sizeof (*sse));
      sse->cwd = 0x37f;

      if (_cpu.features() & FEAT_SSE)
	sse->mxcsr = 0x1f80;

      if (_cpu.has_xsave())
        memset(reinterpret_cast<Xsave_buffer *>(s)->header, 0,
	       sizeof (Xsave_buffer::header));

      static_assert(sizeof (sse_regs) == 512, "SSE-regs size not 512 bytes");
    }
  else
    {
      fpu_regs *fpu = reinterpret_cast<fpu_regs *>(s);

      assert (_state_size >= sizeof (*fpu));
      memset(fpu, 0, sizeof (*fpu));
      fpu->cwd = 0xffff037f;
      fpu->swd = 0xffff0000;
      fpu->twd = 0xffffffff;
      fpu->fos = 0xffff0000;
    }
}

IMPLEMENT
void
Fpu::init(Cpu_number cpu, bool resume)
{
  // At first, noone owns the FPU
  Fpu &f = Fpu::fpu.cpu(cpu);

  init_disable();

  if (cpu == Cpu_number::boot_cpu() && !resume)
    printf("FPU%u: %s%s\n", cxx::int_value<Cpu_number>(cpu),
           Cpu::cpus.cpu(cpu).features() & FEAT_SSE  ? "SSE "  : "",
           Cpu::cpus.cpu(cpu).ext_features() & FEATX_AVX ? "AVX "  : "");

  unsigned cpu_align = 0, cpu_size  = 0;

  if (Cpu::cpus.cpu(cpu).has_xsave())
    {
      init_xsave(cpu);

      Cpu::cpus.cpu(cpu).update_features_info();

      Unsigned32 eax, ecx, edx;
      Cpu::cpus.cpu(cpu).cpuid(0xd, 0, &eax, &cpu_size, &ecx, &edx);
      cpu_align = 64;
      f._variant = Variant_xsave;
    }
  else if (Cpu::have_fxsr())
    {
      cpu_size  = sizeof(sse_regs);
      cpu_align = 16;
      f._variant  = Variant_fxsr;
    }
  else
    {
      cpu_size  = sizeof(fpu_regs);
      cpu_align = 4;
      f._variant  = Variant_fpu;
    }

  if (cpu_size > _state_size)
    _state_size = cpu_size;
  if (cpu_align > _state_align)
    _state_align = cpu_align;

  f.finish_init();
}


/**
 * Return size of FPU context structure, depending on i387 or SSE
 * @return size of FPU context structure
 */
IMPLEMENT inline NEEDS ["cpu.h", "regdefs.h"]
unsigned
Fpu::state_size()
{
  return _state_size;
}

/**
 * Return recommended FPU context alignment, depending on i387 or SSE
 * @return recommended FPU context alignment
 */
IMPLEMENT inline NEEDS ["cpu.h", "regdefs.h"]
unsigned
Fpu::state_align()
{
  return _state_align;
}

//----------------------------------------------------------------
IMPLEMENTATION [fpu && lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{
  set_owner(0);
  disable();
}

//----------------------------------------------------------------
IMPLEMENTATION [fpu && !lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{
  enable();
}
