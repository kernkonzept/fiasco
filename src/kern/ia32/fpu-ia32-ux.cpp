/*
 * Fiasco FPU Code
 * Shared between UX and native IA32.
 */

INTERFACE[ia32,amd64,ux]:

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

IMPLEMENTATION[ia32,amd64,ux]:

#include <cstring>
#include "cpu.h"
#include "fpu_state.h"
#include "regdefs.h"
#include "globals.h"
#include "static_assert.h"

unsigned Fpu::_state_size;
unsigned Fpu::_state_align;

/*
 * Initialize FPU or SSE state
 * We don't use finit, because it is slow. Initializing the context in
 * memory and fetching it via restore_state is supposedly faster
 */
IMPLEMENT inline NEEDS ["cpu.h", "fpu_state.h", "globals.h", "regdefs.h",
                        "static_assert.h", <cstring>]
void
Fpu::init_state(Fpu_state *s)
{
  Cpu const &_cpu = Cpu::cpus.cpu(current_cpu());
  if (_cpu.features() & FEAT_FXSR)
    {
      assert (_state_size >= sizeof (sse_regs));
      sse_regs *sse = reinterpret_cast<sse_regs *>(s->state_buffer());

      memset(sse, 0, sizeof (*sse));
      sse->cwd = 0x37f;

      if (_cpu.features() & FEAT_SSE)
	sse->mxcsr = 0x1f80;

      if (_cpu.has_xsave())
        memset(reinterpret_cast<Xsave_buffer *>(s->state_buffer())->header, 0,
	       sizeof (Xsave_buffer::header));

      static_assert(sizeof (sse_regs) == 512, "SSE-regs size not 512 bytes");
    }
  else
    {
      fpu_regs *fpu = reinterpret_cast<fpu_regs *>(s->state_buffer());

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
  // Mark FPU busy, so that first FPU operation will yield an exception
  disable();

  // At first, noone owns the FPU
  Fpu &f = Fpu::fpu.cpu(cpu);

  f.set_owner(0);

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
