//------------------------------------------------------------------------
INTERFACE [arm_v8]:

EXTENSION class Fpu
{
public:
  struct Exception_state_user
  {
  };
};

//------------------------------------------------------------------------
INTERFACE [arm_v8 && fpu && !arm_sve]:

class Fpu_state_simd
{
public:
  void init();
  void save();
  void restore() const;
  void copy(Fpu_state const *from);

private:
  Unsigned32 _fpcr, _fpsr;
  Unsigned64 _state[64]; // 32 128bit regs
};

class Fpu_state : public Fpu_state_simd {};

//------------------------------------------------------------------------
INTERFACE [arm_v8 && fpu && arm_sve]:

#include <cxx/type_traits>

EXTENSION class Fpu
{
public:
  enum class State_type
  {
    None,
    Simd,
    Sve,
  };

  static constexpr State_type Default_state_type = State_type::Simd;

  static inline bool has_sve()
  { return _has_sve; }

  static inline unsigned max_vl()
  { return _max_vl; }

private:
  /// SVE support detected?
  static bool _has_sve;
  /// Vector length in quad-words (128-bits)
  static unsigned _max_vl;
};

class Fpu_state
{
public:
  virtual Fpu::State_type type() const = 0;
  virtual void init() = 0;
  virtual void save() = 0;
  virtual void restore() const = 0;
  virtual void copy(Fpu_state const *from) = 0;
};

class Fpu_state_simd : public Fpu_state
{
public:
  Fpu::State_type type() const override
  { return Fpu::State_type::Simd; }

  void init() override;
  void save() override;
  void restore() const override;
  void copy(Fpu_state const *from) override;

private:
  Unsigned32 _fpcr, _fpsr;
  Unsigned64 _state[64]; // 32 128bit regs
};

class Fpu_state_sve : public Fpu_state
{
public:
  Fpu::State_type type() const override
  { return Fpu::State_type::Sve; }

  enum
  {
    Num_z_regs = 32,
    Num_p_regs = 16,
  };

  template<typename T, unsigned SIZE_CONST, unsigned SIZE_VL, typename PREV,
           unsigned ALIGN = alignof(T)>
  struct Element
  {
    using Type = T;

    static constexpr unsigned off([[maybe_unused]] unsigned vl)
    {
      if constexpr (cxx::is_same_v<PREV, void>)
        return 0;
      else
        return PREV::off(vl) + PREV::size(vl);
    }

    static constexpr unsigned size(unsigned vl)
    { return SIZE_CONST + vl * SIZE_VL; }

    static_assert(off(1) % ALIGN == 0,
                  "Broken alignment for odd vector lengths.");
    static_assert(off(2) % ALIGN == 0,
                  "Broken alignment for even vector lengths.");
  };

  /// Vector registers Z0..Z31 (must be 16-byte aligned)
  using Z = Element<Unsigned64, 0, Num_z_regs * 16, void, 16>;
  /// SVE control register (holds vector length selected by EL1 userspace)
  using Zcr = Element<Unsigned32, sizeof(Unsigned32), 0, Z>;
  using Fpcr = Element<Unsigned32, sizeof(Unsigned32), 0, Zcr>;
  using Fpsr = Element<Unsigned32, sizeof(Unsigned32), 0, Fpcr>;
  /// Predicate registers P0..P15
  using P = Element<Unsigned16, 0, Num_p_regs * 2, Fpsr>;
  /// First-fault register (same length and layout as a P register)
  using Ffr = Element<Unsigned16, 0, 2, P>;

  static void init_dyn_size()
  {
    using End = Element<Unsigned8, 0, 0, Ffr>;
    _dyn_size = End::off(Fpu::max_vl());
  }

  static unsigned dyn_size()
  {
    return _dyn_size;
  }

  alignas(16) Unsigned8 ext_state[0];

  template<typename E>
  inline typename E::Type *access()
  {
    return reinterpret_cast<typename E::Type *>(ext_state + E::off(Fpu::max_vl()));
  }

  template<typename E>
  inline typename E::Type const *get() const
  {
    return reinterpret_cast<typename E::Type const *>(ext_state + E::off(Fpu::max_vl()));
  }

private:
  static unsigned _dyn_size;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu]:

#include <cassert>
#include <cstdio>
#include <cstring>

#include "cpu.h"
#include "mem.h"
#include "processor.h"
#include "trap_state.h"

IMPLEMENT
void
Fpu::init(Cpu_number cpu, bool resume)
{
  if constexpr (Config::Jdb)
    {
      if (!resume && cpu == Cpu_number::boot_cpu())
        printf("FPU: Initialize\n");
    }

  if (!resume)
    detect_sve(cpu);

  Fpu &f = fpu.cpu(cpu);
  if (!resume)
    show(cpu);

  // make sure that in HYP case CPACR is loaded and enabled.
  // without HYP the disable below will disable it, so this does not hurt
  __asm__ __volatile__ (
      "msr  CPACR_EL1, %[cpacr_on]"
      : : [cpacr_on]"r"(Cpu::Cpacr_el1_generic_hyp));

  f.finish_init();
}

IMPLEMENT inline NEEDS ["mem.h"]
void
Fpu_state_simd::init()
{
  _fpcr = 0;
  _fpsr = 0;
  static_assert(!(sizeof (_state) % sizeof(Mword)),
                "Non-mword size of Fpu_state");
  Mem::memset_mwords(_state, 0, sizeof (_state) / sizeof(Mword));
}

IMPLEMENT inline
void
Fpu_state_simd::save()
{
  Mword fpcr;
  Mword fpsr;
  asm volatile(".arch_extension fp                        \n"
               "stp     q0, q1,   [%[s], #16 *  0]        \n"
               "stp     q2, q3,   [%[s], #16 *  2]        \n"
               "stp     q4, q5,   [%[s], #16 *  4]        \n"
               "stp     q6, q7,   [%[s], #16 *  6]        \n"
               "stp     q8, q9,   [%[s], #16 *  8]        \n"
               "stp     q10, q11, [%[s], #16 * 10]        \n"
               "stp     q12, q13, [%[s], #16 * 12]        \n"
               "stp     q14, q15, [%[s], #16 * 14]        \n"
               "stp     q16, q17, [%[s], #16 * 16]        \n"
               "stp     q18, q19, [%[s], #16 * 18]        \n"
               "stp     q20, q21, [%[s], #16 * 20]        \n"
               "stp     q22, q23, [%[s], #16 * 22]        \n"
               "stp     q24, q25, [%[s], #16 * 24]        \n"
               "stp     q26, q27, [%[s], #16 * 26]        \n"
               "stp     q28, q29, [%[s], #16 * 28]        \n"
               "stp     q30, q31, [%[s], #16 * 30]        \n"
               "mrs     %[fpcr], fpcr                     \n"
               "mrs     %[fpsr], fpsr                     \n"
               ".arch_extension nofp                      \n"
               : [fpcr] "=r" (fpcr),
                 [fpsr] "=r" (fpsr),
                 "=m" (_state)
               : [s] "r" (_state));
  _fpcr = fpcr;
  _fpsr = fpsr;
}

IMPLEMENT inline
void
Fpu_state_simd::restore() const
{
  asm volatile(".arch_extension fp                        \n"
               "ldp     q0, q1,   [%[s], #16 *  0]        \n"
               "ldp     q2, q3,   [%[s], #16 *  2]        \n"
               "ldp     q4, q5,   [%[s], #16 *  4]        \n"
               "ldp     q6, q7,   [%[s], #16 *  6]        \n"
               "ldp     q8, q9,   [%[s], #16 *  8]        \n"
               "ldp     q10, q11, [%[s], #16 * 10]        \n"
               "ldp     q12, q13, [%[s], #16 * 12]        \n"
               "ldp     q14, q15, [%[s], #16 * 14]        \n"
               "ldp     q16, q17, [%[s], #16 * 16]        \n"
               "ldp     q18, q19, [%[s], #16 * 18]        \n"
               "ldp     q20, q21, [%[s], #16 * 20]        \n"
               "ldp     q22, q23, [%[s], #16 * 22]        \n"
               "ldp     q24, q25, [%[s], #16 * 24]        \n"
               "ldp     q26, q27, [%[s], #16 * 26]        \n"
               "ldp     q28, q29, [%[s], #16 * 28]        \n"
               "ldp     q30, q31, [%[s], #16 * 30]        \n"
               "msr     fpcr, %[fpcr]                     \n"
               "msr     fpsr, %[fpsr]                     \n"
               ".arch_extension nofp                      \n"
               : : [fpcr] "r" (Mword{_fpcr}),
                   [fpsr] "r" (Mword{_fpsr}),
                   [s] "r" (_state),
                   "m" (_state));
}

IMPLEMENT inline
void
Fpu_state_simd::copy(Fpu_state const *from)
{
  *this = *nonull_static_cast<Fpu_state_simd const *>(from);
}

IMPLEMENT inline
unsigned
Fpu::state_size()
{
  return sizeof (Fpu_state_simd);
}

IMPLEMENT inline
unsigned
Fpu::state_align()
{ return 16; }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{
  disable();
  set_owner(0);
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !lazy_fpu]:

PRIVATE inline
void
Fpu::finish_init()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu]:

PRIVATE static
void
Fpu::show(Cpu_number)
{}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !cpu_virt]:

PUBLIC static inline NEEDS ["cpu.h"]
bool
Fpu::is_enabled()
{
  Mword x;
  asm volatile ("mrs %0, CPACR_EL1" : "=r"(x));
  return x & Cpu::Cpacr_el1_fpen_full;
}


PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::enable()
{
  Mword t;
  asm volatile("mrs  %0, CPACR_EL1  \n"
               "orr  %0, %0, %1     \n"
               "msr  CPACR_EL1, %0  \n"
               : "=r"(t) : "I" (Cpu::Cpacr_el1_fpen_full));
  Mem::isb();
}

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::disable()
{
  Mword t;
  asm volatile("mrs  %0, CPACR_EL1  \n"
               "bic  %0, %0, %1     \n"
               "msr  CPACR_EL1, %0  \n"
               : "=r"(t) : "I" (Cpu::Cpacr_el1_fpen_full));
  Mem::isb();
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && cpu_virt]:

PUBLIC static inline NEEDS ["cpu.h"]
bool
Fpu::is_enabled()
{
  Mword dummy;
  __asm__ __volatile__ ("mrs %0, CPTR_EL2" : "=r"(dummy));
  return !(dummy & Cpu::Cptr_el2_tfp);
}

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::enable()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrs %0, CPTR_EL2  \n"
      "bic %0, %0, %1    \n"
      "msr CPTR_EL2, %0  \n"
      : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tfp));
  Mem::isb();
}

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::disable()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrs  %0, CPTR_EL2  \n"
      "orr  %0, %0, %1    \n"
      "msr  CPTR_EL2, %0  \n"
      : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tfp));
  Mem::isb();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !arm_sve]:

IMPLEMENT
void
Fpu::init_state(Fpu_state *fpu_state)
{ fpu_state->init(); }

PRIVATE static inline
void
Fpu::detect_sve(Cpu_number)
{}

IMPLEMENT_DEFAULT
void
Fpu::save_state(Fpu_state *fpu_state)
{
  assert(fpu_state);
  fpu_state->save();
}

IMPLEMENT_DEFAULT
void
Fpu::restore_state(Fpu_state const *fpu_state)
{
  assert(fpu_state);
  fpu_state->restore();
}

IMPLEMENT_OVERRIDE inline
void Fpu::copy_state(Fpu_state *to, Fpu_state const *from)
{
  to->copy(from);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && arm_sve]:

#include "panic.h"

bool Fpu::_has_sve = false;
unsigned Fpu::_max_vl = 0;
unsigned Fpu_state_sve::_dyn_size = 0;

PUBLIC static inline
unsigned
Fpu::state_size(Fpu::State_type type)
{
  if (type == Fpu::State_type::Sve)
    return sizeof(Fpu_state_sve) + Fpu_state_sve::dyn_size();
  else
    return sizeof(Fpu_state_simd);
}

PRIVATE static
void
Fpu::detect_sve(Cpu_number cpu)
{
  bool has_sve = Cpu::cpus.cpu(cpu).has_sve();
  unsigned max_vl = 0;
  if (has_sve)
    {
      // Enable both SVE and FP/SIMD, because if we only enable SVE, the FP/SIMD
      // trap mechanism will trigger when we access the ZCR register or execute
      // the rdvl instruction.
      enable_sve();
      bool fpsimd_enabled = is_enabled();
      if (!fpsimd_enabled)
        enable();

      // Detect maximum supported VL
      Cpu::zcr(Cpu::Zcr_vl_max);
      max_vl = Cpu::sve_vl();

      // At least 128-bit, the FP/SIMD registers, must be present
      if (max_vl < 1)
        panic("Minimum required SVE vector length not available!");

      Cpu::zcr(max_vl - 1);

      disable_sve();
      if (!fpsimd_enabled)
        disable();
    }

  if (cpu == Cpu_number::boot_cpu())
    {
      if (has_sve)
        printf("CPU implements SVE with a vector length of up to %u bits.\n",
               max_vl * 128);

      _has_sve = has_sve;
      _max_vl = max_vl;
      Fpu_state_sve::init_dyn_size();
    }
  else
    {
      // We assume that all CPUs exhibit the same SVE limits.
      assert(_has_sve == has_sve);
      assert(_max_vl == max_vl);
    }
}

PUBLIC static inline
void
Fpu::init_state(Fpu_state *fpu_state, Fpu::State_type type)
{
  if (type == Fpu::State_type::Simd)
    new (fpu_state) Fpu_state_simd();
  else
    new (fpu_state) Fpu_state_sve();

  fpu_state->init();
}

PUBLIC static inline
void
Fpu::init_sve_from_simd(Fpu_state *fpu_state)
{
  assert(fpu_state->type() == Fpu::State_type::Sve);
  enable_sve();
  nonull_static_cast<Fpu_state_sve *>(fpu_state)->init_from_simd();
}

IMPLEMENT
void
Fpu::save_state(Fpu_state *fpu_state)
{
  assert(fpu_state);
  fpu_state->save();
}

IMPLEMENT
void
Fpu::restore_state(Fpu_state const *fpu_state)
{
  assert(fpu_state);

  // Enable/Disable SVE depending on whether the state we are about to restore
  // is an SVE state or an FP/SIMD state. In fact, this is the only place where
  // the SVE trap flag is toggled. But since the regular FP/SIMD trap also
  // traps the execution of SVE instructions and whenever the FPU ownership
  // changes, restore_state() is called. This is sufficient for both eager and
  // lazy FPU switching.
  if (fpu_state->type() == Fpu::State_type::Sve)
    enable_sve();
  else
    disable_sve();

  fpu_state->restore();
}

IMPLEMENT_OVERRIDE inline
void Fpu::copy_state(Fpu_state *to, Fpu_state const *from)
{
  assert(to->type() == from->type());
  to->copy(from);
}

PUBLIC
void
Fpu_state_sve::init() override
{
  memset(ext_state, 0, _dyn_size);

  // Default to maximum vector length
  *access<Zcr>() = Fpu::max_vl() - 1;
}

PROTECTED inline
void
Fpu_state_sve::save_regs()
{
  Mword fpcr;
  Mword fpsr;
  asm volatile(// Vector registers
               ".arch_extension sve             \n"
               "str z0,  [%[z], #0, mul vl]     \n"
               "str z1,  [%[z], #1, mul vl]     \n"
               "str z2,  [%[z], #2, mul vl]     \n"
               "str z3,  [%[z], #3, mul vl]     \n"
               "str z4,  [%[z], #4, mul vl]     \n"
               "str z5,  [%[z], #5, mul vl]     \n"
               "str z6,  [%[z], #6, mul vl]     \n"
               "str z7,  [%[z], #7, mul vl]     \n"
               "str z8,  [%[z], #8, mul vl]     \n"
               "str z9,  [%[z], #9, mul vl]     \n"
               "str z10, [%[z], #10, mul vl]    \n"
               "str z11, [%[z], #11, mul vl]    \n"
               "str z12, [%[z], #12, mul vl]    \n"
               "str z13, [%[z], #13, mul vl]    \n"
               "str z14, [%[z], #14, mul vl]    \n"
               "str z15, [%[z], #15, mul vl]    \n"
               "str z16, [%[z], #16, mul vl]    \n"
               "str z17, [%[z], #17, mul vl]    \n"
               "str z18, [%[z], #18, mul vl]    \n"
               "str z19, [%[z], #19, mul vl]    \n"
               "str z20, [%[z], #20, mul vl]    \n"
               "str z21, [%[z], #21, mul vl]    \n"
               "str z22, [%[z], #22, mul vl]    \n"
               "str z23, [%[z], #23, mul vl]    \n"
               "str z24, [%[z], #24, mul vl]    \n"
               "str z25, [%[z], #25, mul vl]    \n"
               "str z26, [%[z], #26, mul vl]    \n"
               "str z27, [%[z], #27, mul vl]    \n"
               "str z28, [%[z], #28, mul vl]    \n"
               "str z29, [%[z], #29, mul vl]    \n"
               "str z30, [%[z], #30, mul vl]    \n"
               "str z31, [%[z], #31, mul vl]    \n"

               // Predicate registers
               "str p0,  [%[p], #0, mul vl]     \n"
               "str p1,  [%[p], #1, mul vl]     \n"
               "str p2,  [%[p], #2, mul vl]     \n"
               "str p3,  [%[p], #3, mul vl]     \n"
               "str p4,  [%[p], #4, mul vl]     \n"
               "str p5,  [%[p], #5, mul vl]     \n"
               "str p6,  [%[p], #6, mul vl]     \n"
               "str p7,  [%[p], #7, mul vl]     \n"
               "str p8,  [%[p], #8, mul vl]     \n"
               "str p9,  [%[p], #9, mul vl]     \n"
               "str p10, [%[p], #10, mul vl]    \n"
               "str p11, [%[p], #11, mul vl]    \n"
               "str p12, [%[p], #12, mul vl]    \n"
               "str p13, [%[p], #13, mul vl]    \n"
               "str p14, [%[p], #14, mul vl]    \n"
               "str p15, [%[p], #15, mul vl]    \n"

               // FFR
               "rdffr p0.b                      \n"
               "str p0,  [%[ffr], #0, mul vl]   \n"
               "ldr p0,  [%[p], #0, mul vl]     \n"
               ".arch_extension nosve           \n"

               "mrs     %[fpcr], fpcr           \n"
               "mrs     %[fpsr], fpsr           \n"
               : [fpcr] "=r" (fpcr),
                 [fpsr] "=r" (fpsr)
               : [z] "r" (access<Z>()),
                 [p] "r" (access<P>()),
                 [ffr] "r" (access<Ffr>()));

  *access<Fpcr>() = fpcr;
  *access<Fpsr>() = fpsr;
}

PROTECTED inline
void
Fpu_state_sve::restore_regs() const
{
  asm volatile(// Vector registers
               ".arch_extension sve             \n"
               "ldr z0,  [%[z], #0, mul vl]     \n"
               "ldr z1,  [%[z], #1, mul vl]     \n"
               "ldr z2,  [%[z], #2, mul vl]     \n"
               "ldr z3,  [%[z], #3, mul vl]     \n"
               "ldr z4,  [%[z], #4, mul vl]     \n"
               "ldr z5,  [%[z], #5, mul vl]     \n"
               "ldr z6,  [%[z], #6, mul vl]     \n"
               "ldr z7,  [%[z], #7, mul vl]     \n"
               "ldr z8,  [%[z], #8, mul vl]     \n"
               "ldr z9,  [%[z], #9, mul vl]     \n"
               "ldr z10, [%[z], #10, mul vl]    \n"
               "ldr z11, [%[z], #11, mul vl]    \n"
               "ldr z12, [%[z], #12, mul vl]    \n"
               "ldr z13, [%[z], #13, mul vl]    \n"
               "ldr z14, [%[z], #14, mul vl]    \n"
               "ldr z15, [%[z], #15, mul vl]    \n"
               "ldr z16, [%[z], #16, mul vl]    \n"
               "ldr z17, [%[z], #17, mul vl]    \n"
               "ldr z18, [%[z], #18, mul vl]    \n"
               "ldr z19, [%[z], #19, mul vl]    \n"
               "ldr z20, [%[z], #20, mul vl]    \n"
               "ldr z21, [%[z], #21, mul vl]    \n"
               "ldr z22, [%[z], #22, mul vl]    \n"
               "ldr z23, [%[z], #23, mul vl]    \n"
               "ldr z24, [%[z], #24, mul vl]    \n"
               "ldr z25, [%[z], #25, mul vl]    \n"
               "ldr z26, [%[z], #26, mul vl]    \n"
               "ldr z27, [%[z], #27, mul vl]    \n"
               "ldr z28, [%[z], #28, mul vl]    \n"
               "ldr z29, [%[z], #29, mul vl]    \n"
               "ldr z30, [%[z], #30, mul vl]    \n"
               "ldr z31, [%[z], #31, mul vl]    \n"

               // FFR
               "ldr p0,  [%[ffr], #0, mul vl]   \n"
               "wrffr p0.b                      \n"

               // Predicate registers
               "ldr p0,  [%[p], #0, mul vl]     \n"
               "ldr p1,  [%[p], #1, mul vl]     \n"
               "ldr p2,  [%[p], #2, mul vl]     \n"
               "ldr p3,  [%[p], #3, mul vl]     \n"
               "ldr p4,  [%[p], #4, mul vl]     \n"
               "ldr p5,  [%[p], #5, mul vl]     \n"
               "ldr p6,  [%[p], #6, mul vl]     \n"
               "ldr p7,  [%[p], #7, mul vl]     \n"
               "ldr p8,  [%[p], #8, mul vl]     \n"
               "ldr p9,  [%[p], #9, mul vl]     \n"
               "ldr p10, [%[p], #10, mul vl]    \n"
               "ldr p11, [%[p], #11, mul vl]    \n"
               "ldr p12, [%[p], #12, mul vl]    \n"
               "ldr p13, [%[p], #13, mul vl]    \n"
               "ldr p14, [%[p], #14, mul vl]    \n"
               "ldr p15, [%[p], #15, mul vl]    \n"
               ".arch_extension nosve           \n"

               "msr     fpcr, %[fpcr]           \n"
               "msr     fpsr, %[fpsr]           \n"
               : : [z] "r" (get<Z>()),
                   [p] "r" (get<P>()),
                   [ffr] "r" (get<Ffr>()),
                   [fpcr] "r" (get<Fpcr>()),
                   [fpsr] "r" (get<Fpsr>()));
}

PUBLIC inline
void
Fpu_state_sve::init_from_simd()
{
  auto zcr = Cpu::zcr();
  // Drop ZCR temporarily to the size of FP/SIMD registers (128-bit)
  Cpu::zcr(Cpu::Zcr_vl_128);
  save_regs();
  // Restore previous ZCR
  Cpu::zcr(zcr);
}

PUBLIC inline NEEDS[<cstring>]
void
Fpu_state_sve::copy(Fpu_state const *from) override
{
  memcpy(ext_state, nonull_static_cast<Fpu_state_sve const *>(from)->ext_state,
         _dyn_size);
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && !cpu_virt && arm_sve]:

PUBLIC
void
Fpu_state_sve::save() override
{ save_regs(); }

PUBLIC
void
Fpu_state_sve::restore() const override
{ restore_regs(); }

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::enable_sve()
{
  Mword t;
  asm volatile("mrs  %0, CPACR_EL1  \n"
               "orr  %0, %0, %1     \n"
               "msr  CPACR_EL1, %0  \n"
               : "=r"(t) : "I" (Cpu::Cpacr_el1_zen_full));
  Mem::isb();
}

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::disable_sve()
{
  Mword t;
  asm volatile("mrs  %0, CPACR_EL1  \n"
               "bic  %0, %0, %1     \n"
               "msr  CPACR_EL1, %0  \n"
               : "=r"(t) : "I" (Cpu::Cpacr_el1_zen_full));
  // No need for an ISB here, as returning to user mode already acts as
  // a context synchronization event.
}

//-------------------------------------------------------------------------
IMPLEMENTATION [arm && fpu && cpu_virt && arm_sve]:

PRIVATE static template<typename CB> inline
void
Fpu_state_sve::with_adjusted_vl(Mword sel_zcr, CB &&cb)
{
  // Vector length selected by user mode
  unsigned sel_zcr_vl = (sel_zcr & Cpu::Zcr_vl_mask);
  // Maximum vector length supported by the CPU and Fiasco
  unsigned max_zcr_vl = Fpu::max_vl() - 1;
  // Avoid operating on unused bytes in the SVE registers if user mode uses a
  // smaller vector length than the maximum supported by Fiasco!
  bool drop_vl = sel_zcr_vl < max_zcr_vl;
  if (drop_vl)
    Cpu::zcr(sel_zcr_vl);

  cb();

  // Restore maximum vector length
  if (drop_vl)
    Cpu::zcr(max_zcr_vl);
}

PUBLIC
void
Fpu_state_sve::save() override
{
  // Save vector length selected by user mode
  Mword zcr_el1 = Cpu::zcr_el1();
  *access<Zcr>() = zcr_el1;

  with_adjusted_vl(zcr_el1, [=]() { save_regs(); });
}

PUBLIC
void
Fpu_state_sve::restore() const override
{
  Mword zcr_el1 = *get<Zcr>();

  with_adjusted_vl(zcr_el1, [=]() { restore_regs(); });

  // Restore vector length selected by user mode
  Cpu::zcr_el1(zcr_el1);
}

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::enable_sve()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrs %0, CPTR_EL2         \n"
      "bic %0, %0, %1           \n"
      "msr CPTR_EL2, %0         \n"
      : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tz));
  Mem::isb();
}

PUBLIC static inline NEEDS ["cpu.h"]
void
Fpu::disable_sve()
{
  Mword dummy;
  __asm__ __volatile__ (
      "mrs  %0, CPTR_EL2           \n"
      "orr  %0, %0, %1             \n"
      "msr  CPTR_EL2, %0           \n"
      : "=&r" (dummy) : "I" (Cpu::Cptr_el2_tz));
  // No need for an ISB here, as returning to user mode already acts as
  // a context synchronization event.
}
