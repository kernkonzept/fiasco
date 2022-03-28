INTERFACE [arm && 64bit && mpu && arm_v8]:

struct Mpu_arm_el1
{
  static void init()
  {
    // Do not touch region 0. We might execute from it!
    for (auto i = regions(); i > 1; i--)
      {
        prselr(i-1);
        prlar(0);
      }
  }

  static Mword regions()
  {
    Mword v;
    asm("mrs %0, S3_0_c0_c0_4" : "=r"(v)); // MPUIR_EL1
    return v;
  }

  static void prbar(Mword v)
  { asm volatile("msr S3_0_c6_c8_0, %0" : : "r"(v)); } // PRBAR_EL1

  static Mword prlar()
  {
    Mword v;
    asm volatile("mrs %0, S3_0_c6_c8_1" : "=r"(v)); // PRLAR_EL1
    return v;
  }

  static void prlar(Mword v)
  { asm volatile("msr S3_0_c6_c8_1, %0" : : "r"(v)); } // PRLAR_EL1

  static Mword prselr() // PRSELR_EL1
  {
    Mword v;
    asm volatile("mrs %0, S3_0_c6_c2_1" : "=r"(v));
    return v;
  }

  static void prselr(Mword v) // PRSELR_EL1
  {
    asm volatile(
      "msr S3_0_c6_c2_1, %0  \n"
      "isb                   \n"
      : : "r"(v));
  }

  static Mword prenr()
  {
    Mword v;
    asm volatile("mrs %0, S3_0_c6_c1_1" : "=r"(v)); // PRENR_EL1
    return v;
  }

  static void prenr(Mword v)
  { asm volatile("msr S3_0_c6_c1_1, %0" : : "r"(v)); } // PRENR_EL1

  static Mword prbar0()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c8_0" : "=r"(v)); return v; }
  static Mword prbar1()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c8_4" : "=r"(v)); return v; }
  static Mword prbar2()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c9_0" : "=r"(v)); return v; }
  static Mword prbar3()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c9_4" : "=r"(v)); return v; }
  static Mword prbar4()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c10_0" : "=r"(v)); return v; }
  static Mword prbar5()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c10_4" : "=r"(v)); return v; }
  static Mword prbar6()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c11_0" : "=r"(v)); return v; }
  static Mword prbar7()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c11_4" : "=r"(v)); return v; }
  static Mword prbar8()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c12_0" : "=r"(v)); return v; }
  static Mword prbar9()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c12_4" : "=r"(v)); return v; }
  static Mword prbar10()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c13_0" : "=r"(v)); return v; }
  static Mword prbar11()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c13_4" : "=r"(v)); return v; }
  static Mword prbar12()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c14_0" : "=r"(v)); return v; }
  static Mword prbar13()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c14_4" : "=r"(v)); return v; }
  static Mword prbar14()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c15_0" : "=r"(v)); return v; }
  static Mword prbar15()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c15_4" : "=r"(v)); return v; }

  static void prbar0(Mword v)
  { asm volatile("msr S3_0_c6_c8_0, %0" : : "r"(v)); }
  static void prbar1(Mword v)
  { asm volatile("msr S3_0_c6_c8_4, %0" : : "r"(v)); }
  static void prbar2(Mword v)
  { asm volatile("msr S3_0_c6_c9_0, %0" : : "r"(v)); }
  static void prbar3(Mword v)
  { asm volatile("msr S3_0_c6_c9_4, %0" : : "r"(v)); }
  static void prbar4(Mword v)
  { asm volatile("msr S3_0_c6_c10_0, %0" : : "r"(v)); }
  static void prbar5(Mword v)
  { asm volatile("msr S3_0_c6_c10_4, %0" : : "r"(v)); }
  static void prbar6(Mword v)
  { asm volatile("msr S3_0_c6_c11_0, %0" : : "r"(v)); }
  static void prbar7(Mword v)
  { asm volatile("msr S3_0_c6_c11_4, %0" : : "r"(v)); }
  static void prbar8(Mword v)
  { asm volatile("msr S3_0_c6_c12_0, %0" : : "r"(v)); }
  static void prbar9(Mword v)
  { asm volatile("msr S3_0_c6_c12_4, %0" : : "r"(v)); }
  static void prbar10(Mword v)
  { asm volatile("msr S3_0_c6_c13_0, %0" : : "r"(v)); }
  static void prbar11(Mword v)
  { asm volatile("msr S3_0_c6_c13_4, %0" : : "r"(v)); }
  static void prbar12(Mword v)
  { asm volatile("msr S3_0_c6_c14_0, %0" : : "r"(v)); }
  static void prbar13(Mword v)
  { asm volatile("msr S3_0_c6_c14_4, %0" : : "r"(v)); }
  static void prbar14(Mword v)
  { asm volatile("msr S3_0_c6_c15_0, %0" : : "r"(v)); }
  static void prbar15(Mword v)
  { asm volatile("msr S3_0_c6_c15_4, %0" : : "r"(v)); }

  static Mword prlar0()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c8_1" : "=r"(v)); return v; }
  static Mword prlar1()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c8_5" : "=r"(v)); return v; }
  static Mword prlar2()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c9_1" : "=r"(v)); return v; }
  static Mword prlar3()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c9_5" : "=r"(v)); return v; }
  static Mword prlar4()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c10_1" : "=r"(v)); return v; }
  static Mword prlar5()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c10_5" : "=r"(v)); return v; }
  static Mword prlar6()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c11_1" : "=r"(v)); return v; }
  static Mword prlar7()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c11_5" : "=r"(v)); return v; }
  static Mword prlar8()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c12_1" : "=r"(v)); return v; }
  static Mword prlar9()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c12_5" : "=r"(v)); return v; }
  static Mword prlar10()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c13_1" : "=r"(v)); return v; }
  static Mword prlar11()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c13_5" : "=r"(v)); return v; }
  static Mword prlar12()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c14_1" : "=r"(v)); return v; }
  static Mword prlar13()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c14_5" : "=r"(v)); return v; }
  static Mword prlar14()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c15_1" : "=r"(v)); return v; }
  static Mword prlar15()
  { Mword v; asm volatile("mrs %0, S3_0_c6_c15_5" : "=r"(v)); return v; }

  static void prlar0(Mword v)
  { asm volatile("msr S3_0_c6_c8_1, %0" : : "r"(v)); }
  static void prlar1(Mword v)
  { asm volatile("msr S3_0_c6_c8_5, %0" : : "r"(v)); }
  static void prlar2(Mword v)
  { asm volatile("msr S3_0_c6_c9_1, %0" : : "r"(v)); }
  static void prlar3(Mword v)
  { asm volatile("msr S3_0_c6_c9_5, %0" : : "r"(v)); }
  static void prlar4(Mword v)
  { asm volatile("msr S3_0_c6_c10_1, %0" : : "r"(v)); }
  static void prlar5(Mword v)
  { asm volatile("msr S3_0_c6_c10_5, %0" : : "r"(v)); }
  static void prlar6(Mword v)
  { asm volatile("msr S3_0_c6_c11_1, %0" : : "r"(v)); }
  static void prlar7(Mword v)
  { asm volatile("msr S3_0_c6_c11_5, %0" : : "r"(v)); }
  static void prlar8(Mword v)
  { asm volatile("msr S3_0_c6_c12_1, %0" : : "r"(v)); }
  static void prlar9(Mword v)
  { asm volatile("msr S3_0_c6_c12_5, %0" : : "r"(v)); }
  static void prlar10(Mword v)
  { asm volatile("msr S3_0_c6_c13_1, %0" : : "r"(v)); }
  static void prlar11(Mword v)
  { asm volatile("msr S3_0_c6_c13_5, %0" : : "r"(v)); }
  static void prlar12(Mword v)
  { asm volatile("msr S3_0_c6_c14_1, %0" : : "r"(v)); }
  static void prlar13(Mword v)
  { asm volatile("msr S3_0_c6_c14_5, %0" : : "r"(v)); }
  static void prlar14(Mword v)
  { asm volatile("msr S3_0_c6_c15_1, %0" : : "r"(v)); }
  static void prlar15(Mword v)
  { asm volatile("msr S3_0_c6_c15_5, %0" : : "r"(v)); }
};

struct Mpu_arm_el2
{
  static void init()
  {
    // Do not touch region 0. We might execute from it!
    for (auto i = regions(); i > 1; i--)
      {
        prselr(i-1);
        prlar(0);
      }
  }

  static Mword regions()
  {
    Mword v;
    asm("mrs %0, S3_4_c0_c0_4" : "=r"(v)); // MPUIR_EL2
    return v;
  }

  static void prbar(Mword v)
  { asm volatile("msr S3_4_c6_c8_0, %0" : : "r"(v)); } // PRBAR_EL2

  static Mword prlar()
  {
    Mword v;
    asm volatile("mrs %0, S3_4_c6_c8_1" : "=r"(v)); // PRLAR_EL2
    return v;
  }

  static void prlar(Mword v)
  { asm volatile("msr S3_4_c6_c8_1, %0" : : "r"(v)); } // PRLAR_EL2

  static void prselr(Mword v) // PRSELR_EL2
  {
    asm volatile(
      "msr S3_4_c6_c2_1, %0  \n"
      "isb                   \n"
      : : "r"(v));
  }

  static Mword prenr()
  {
    Mword v;
    asm volatile("mrs %0, S3_4_c6_c1_1" : "=r"(v)); // PRENR_EL2
    return v;
  }

  static void prenr(Mword v)
  { asm volatile("msr S3_4_c6_c1_1, %0" : : "r"(v)); } // PRENR_EL2

  static void prbar0(Mword v)
  { asm volatile("msr S3_4_c6_c8_0, %0" : : "r"(v)); }
  static void prbar1(Mword v)
  { asm volatile("msr S3_4_c6_c8_4, %0" : : "r"(v)); }
  static void prbar2(Mword v)
  { asm volatile("msr S3_4_c6_c9_0, %0" : : "r"(v)); }
  static void prbar3(Mword v)
  { asm volatile("msr S3_4_c6_c9_4, %0" : : "r"(v)); }
  static void prbar4(Mword v)
  { asm volatile("msr S3_4_c6_c10_0, %0" : : "r"(v)); }
  static void prbar5(Mword v)
  { asm volatile("msr S3_4_c6_c10_4, %0" : : "r"(v)); }
  static void prbar6(Mword v)
  { asm volatile("msr S3_4_c6_c11_0, %0" : : "r"(v)); }
  static void prbar7(Mword v)
  { asm volatile("msr S3_4_c6_c11_4, %0" : : "r"(v)); }
  static void prbar8(Mword v)
  { asm volatile("msr S3_4_c6_c12_0, %0" : : "r"(v)); }
  static void prbar9(Mword v)
  { asm volatile("msr S3_4_c6_c12_4, %0" : : "r"(v)); }
  static void prbar10(Mword v)
  { asm volatile("msr S3_4_c6_c13_0, %0" : : "r"(v)); }
  static void prbar11(Mword v)
  { asm volatile("msr S3_4_c6_c13_4, %0" : : "r"(v)); }
  static void prbar12(Mword v)
  { asm volatile("msr S3_4_c6_c14_0, %0" : : "r"(v)); }
  static void prbar13(Mword v)
  { asm volatile("msr S3_4_c6_c14_4, %0" : : "r"(v)); }
  static void prbar14(Mword v)
  { asm volatile("msr S3_4_c6_c15_0, %0" : : "r"(v)); }
  static void prbar15(Mword v)
  { asm volatile("msr S3_4_c6_c15_4, %0" : : "r"(v)); }

  static void prlar0(Mword v)
  { asm volatile("msr S3_4_c6_c8_1, %0" : : "r"(v)); }
  static void prlar1(Mword v)
  { asm volatile("msr S3_4_c6_c8_5, %0" : : "r"(v)); }
  static void prlar2(Mword v)
  { asm volatile("msr S3_4_c6_c9_1, %0" : : "r"(v)); }
  static void prlar3(Mword v)
  { asm volatile("msr S3_4_c6_c9_5, %0" : : "r"(v)); }
  static void prlar4(Mword v)
  { asm volatile("msr S3_4_c6_c10_1, %0" : : "r"(v)); }
  static void prlar5(Mword v)
  { asm volatile("msr S3_4_c6_c10_5, %0" : : "r"(v)); }
  static void prlar6(Mword v)
  { asm volatile("msr S3_4_c6_c11_1, %0" : : "r"(v)); }
  static void prlar7(Mword v)
  { asm volatile("msr S3_4_c6_c11_5, %0" : : "r"(v)); }
  static void prlar8(Mword v)
  { asm volatile("msr S3_4_c6_c12_1, %0" : : "r"(v)); }
  static void prlar9(Mword v)
  { asm volatile("msr S3_4_c6_c12_5, %0" : : "r"(v)); }
  static void prlar10(Mword v)
  { asm volatile("msr S3_4_c6_c13_1, %0" : : "r"(v)); }
  static void prlar11(Mword v)
  { asm volatile("msr S3_4_c6_c13_5, %0" : : "r"(v)); }
  static void prlar12(Mword v)
  { asm volatile("msr S3_4_c6_c14_1, %0" : : "r"(v)); }
  static void prlar13(Mword v)
  { asm volatile("msr S3_4_c6_c14_5, %0" : : "r"(v)); }
  static void prlar14(Mword v)
  { asm volatile("msr S3_4_c6_c15_1, %0" : : "r"(v)); }
  static void prlar15(Mword v)
  { asm volatile("msr S3_4_c6_c15_5, %0" : : "r"(v)); }
};

//------------------------------------------------------------------
INTERFACE [arm && 64bit && mpu && arm_v8 && !cpu_virt]:

typedef Mpu_arm_el1 Mpu_arm;

//------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && mpu && arm_v8 && !cpu_virt]:

#include "cpu.h"

PUBLIC static
bool
Mpu::enabled()
{
  unsigned long control;
  asm ("mrs %0, SCTLR_EL1" : "=r" (control));
  return control & Cpu::Cp15_c1_mmu;
}

PUBLIC static
void
Mpu::enable()
{
  unsigned long control = Cpu::Sctlr_generic;

  asm volatile ("msr MAIR_EL1, %0" : : "r"(Mpu::Mair_bits));
  Mem::dsb();
  asm volatile("msr SCTLR_EL1, %[control]" : : [control] "r" (control));
}

//------------------------------------------------------------------
INTERFACE [arm && 64bit && mpu && arm_v8 && cpu_virt]:

typedef Mpu_arm_el2 Mpu_arm;

//------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && mpu && arm_v8 && cpu_virt]:

#include "cpu.h"

PUBLIC static
bool
Mpu::enabled()
{
  unsigned long control;
  asm ("mrs %0, SCTLR_EL2" : "=r" (control));
  return control & Cpu::Cp15_c1_mmu;
}

PUBLIC static
void
Mpu::enable()
{
  unsigned long control = Cpu::Sctlr_generic;

  asm volatile ("msr MAIR_EL2, %0" : : "r"(Mpu::Mair_bits));
  Mem::dsb();
  asm volatile("msr SCTLR_EL2, %[control]" : : [control] "r" (control));
}

//------------------------------------------------------------------
INTERFACE [arm && 64bit && mpu && arm_v8]:

#include "cpu_lock.h"

EXTENSION struct Mpu_region
{
public:
  Mword prbar, prlar;

  struct Prot {
    enum {
      NX = 1U << 1,
      EL0 = 1U << 2,
      RO = 1U << 3,
      ISH = 3U << 4,
    };
  };

  struct Attr {
    enum {
      // See Mair_bits bits below
      Mask      = 7U << 1,
      Normal    = 0U << 1,  // Normal Outer Non-cachable, Inner Write-Back
      Device    = 1U << 1,  // Device nGnRnE
      Buffered  = 2U << 1,  // Normal Outer+Inner Non-cacheable
    };
  };

  enum { Enabled = 1 };

  constexpr Mword start() { return prbar & ~0x3fU; }
  constexpr Mword end()   { return prlar |  0x3fU; }

  constexpr Mpu_region_attr attr()
  {
    return Mpu_region_attr::make_attr(
              L4_fpage::Rights::R()
                  | ((prbar & Prot::EL0) ? L4_fpage::Rights::U()
                                         : L4_fpage::Rights())
                  | ((prbar & Prot::RO) ? L4_fpage::Rights()
                                        : L4_fpage::Rights::W())
                  | ((prbar & Prot::NX) ? L4_fpage::Rights()
                                        : L4_fpage::Rights::X()),
              ((prlar & Attr::Mask) == Attr::Normal)
               ? L4_msg_item::Memory_type::Normal()
               : (((prlar & Attr::Mask) == Attr::Device)
                  ? L4_msg_item::Memory_type::Uncached()
                  : L4_msg_item::Memory_type::Buffered()),
              prlar & Enabled);
  }

  inline void start(Mword start)
  {
    prbar = (prbar & 0x3fU) | (start & ~0x3fU);
  }

  inline void end(Mword end)
  {
    prlar = (prlar & 0x3fU) | (end & ~0x3fU);
  }

  inline void attr(Mpu_region_attr attr)
  {
    prbar = (prbar & ~0x3fU)
            | ((attr.rights() & L4_fpage::Rights::U()) ? Prot::EL0 : 0)
            | ((attr.rights() & L4_fpage::Rights::W()) ? 0 : Prot::RO)
            | ((attr.rights() & L4_fpage::Rights::X()) ? 0 : Prot::NX)
            | Prot::ISH;
    prlar = (prlar & ~0x3fU)
            | ((attr.type() == L4_msg_item::Memory_type::Uncached())
                ? Attr::Device
                : ((attr.type() == L4_msg_item::Memory_type::Buffered())
                    ? Attr::Buffered
                    : Attr::Normal))
            | (attr.enabled() ? Enabled : 0);
  }

  inline void disable()
  {
    prlar &= ~(Mword)Enabled;
  }

  constexpr Mpu_region() : prev(this), next(this), prbar(~0x3fUL), prlar(0) {}

  Mpu_region(Mword start, Mword end, Mpu_region_attr a)
  : prev(this), next(this), prbar(start & ~0x3fU), prlar(end & ~0x3fU)
  { attr(a); }
};


EXTENSION class Mpu
{
public:
  enum {
    // 0 = Normal Outer+Inner Write-Back, R+W-Allocate
    // 1 = Device nGnRnE
    // 2 = Normal Outer+Inner Non-cacheable
    Mair_bits = 0x4400ffU,
  };
};

IMPLEMENT static inline
unsigned Mpu::regions()
{
  return Mpu_arm::regions();
}


//------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit && mpu && arm_v8]:

IMPLEMENT static
void Mpu::init()
{
  Mpu_arm::init();
}

IMPLEMENT static
void Mpu::sync(Mpu_regions *regions, Unsigned32 touched, bool inplace)
{
  Mpu_regions const &r = *regions;

  int slot;
  while ((slot = __builtin_ffsl(touched)))
    {
      slot -= 1;
      touched &= ~(1UL << slot);

      Mpu_arm::prselr(slot);
      if (!inplace && (Mpu_arm::prlar() & Mpu_region::Enabled))
        {
          // Always disable first! Otherwise a colliding region might
          // exist briefly after writing prbar!
          Mpu_arm::prlar(0);
          asm volatile("isb");
        }

      Mpu_arm::prbar(r[slot]->prbar);
      Mpu_arm::prlar(r[slot]->prlar);
    }

  // PRSELR must be 0 because it's assumed by kernel entry/exit code
  Mpu_arm::prselr(0);
}

IMPLEMENT static
void Mpu::update(Mpu_regions *regions)
{
  Mpu_regions const &r = *regions;
  Unsigned32 reserved = r.reserved();

  // Disable regions that we're updating. Otherwise there is the possiblity to
  // have an invalid, colliding region when prbar is updated and the current
  // prlar of the updated region is still enabled.
  Mpu_arm::prenr(Mpu_arm::prenr() & reserved);

#define UPDATE(i) \
  do \
    { \
      if (!(reserved & (1U << (idx)))) \
        { \
          Mpu_arm::prbar##i(r[idx]->prbar); \
          Mpu_arm::prlar##i(r[idx]->prlar); \
        } \
      --idx; \
    } \
  while (false)

  int idx = regions->size() - 1;
  while (idx >= 0)
    {
      Mpu_arm::prselr(idx & 0xf0);
      switch (idx & 0x0f)
        {
          case 15: UPDATE(15); // fall through
          case 14: UPDATE(14); // fall through
          case 13: UPDATE(13); // fall through
          case 12: UPDATE(12); // fall through
          case 11: UPDATE(11); // fall through
          case 10: UPDATE(10); // fall through
          case  9: UPDATE(9);  // fall through
          case  8: UPDATE(8);  // fall through
          case  7: UPDATE(7);  // fall through
          case  6: UPDATE(6);  // fall through
          case  5: UPDATE(5);  // fall through
          case  4: UPDATE(4);  // fall through
          case  3: UPDATE(3);  // fall through
          case  2: UPDATE(2);  // fall through
          case  1: UPDATE(1);  // fall through
          case  0: UPDATE(0);
            break;
        }
    }

  // PRSELR must be 0 because it's assumed by kernel entry/exit code
#undef UPDATE
}
