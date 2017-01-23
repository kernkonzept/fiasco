/*
 * Fiasco ia32
 * Architecture specific floating point unit code
 */

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
  Fpu::fpu.cpu(cpu)._xcr0 = (((Unsigned64)edx << 32) | eax) & 0xe7;

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
  assert (s->state_buffer());

  // Both fxsave and fnsave are non-waiting instructions and thus
  // cannot cause exception #16 for pending FPU exceptions.

  switch (fpu.current()._variant)
    {
    case Variant_xsave:
      asm volatile("xsave (%2)" : : "a" (~0UL), "d" (~0UL), "r" (s->state_buffer()) : "memory");
      break;
    case Variant_fxsr:
      asm volatile ("fxsave (%0)" : : "r" (s->state_buffer()) : "memory");
      break;
    case Variant_fpu:
      asm volatile ("fnsave (%0)" : : "r" (s->state_buffer()) : "memory");
      break;
    }
}

/*
 * Restore a saved FPU or SSE state
 */
IMPLEMENT inline NEEDS ["globals.h", "globalconfig.h", <cassert>,"regdefs.h",
                        "cpu.h"]
void
Fpu::restore_state(Fpu_state *s)
{
  assert (s->state_buffer());

  // Only fxrstor is a non-waiting instruction and thus
  // cannot cause exception #16 for pending FPU exceptions.
  Fpu &f = fpu.current();

  switch (f._variant)
    {
    case Variant_xsave:
      asm volatile ("xrstor (%2)" : : "a" (~0UL), "d" (~0UL), "r" (s->state_buffer()));
      break;
    case Variant_fxsr:
        {
#if !defined (CONFIG_WORKAROUND_AMD_FPU_LEAK)
          asm volatile ("fxrstor (%0)" : : "r" (s->state_buffer()));
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
              : : "r" (s->state_buffer()), "m" (int_dummy) : "ax");
#endif
        }
      break;
    case Variant_fpu:
      // frstor is a waiting instruction and we must make sure no
      // FPU exceptions are pending here. We distinguish two cases:
      // 1) If we had a previous FPU owner, we called save_state before and
      //    invoked fnsave which re-initialized the FPU and cleared exceptions
      // 2) Otherwise we call fnclex instead to clear exceptions.

      if (!f.owner())
        asm volatile ("fnclex");

      asm volatile ("frstor (%0)" : : "r" (s->state_buffer()));
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
