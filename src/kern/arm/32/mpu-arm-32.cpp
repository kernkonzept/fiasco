INTERFACE [arm && 32bit && mpu && arm_v8]:

#include "mem.h"

struct Mpu_arm_el1
{
  static void init()
  {
    // Do not touch region 0. We might execute from it!
    for (auto i = regions(); i > 1; i--)
      {
        prselr(i-1);
        Mem::isb();
        prlar(0);
      }
  }

  static Mword regions()
  {
    Mword v;
    asm volatile("mrc p15, 0, %0, c0, c0, 4" : "=r"(v)); // MPUIR
    return v >> 8;
  }

  static void prbar(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c3, 0" : : "r"(v)); }

  static Mword prlar()
  {
    Mword v;
    asm volatile("mrc p15, 0, %0, c6, c3, 1" : "=r"(v));
    return v;
  }

  static void prlar(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c3, 1" : : "r"(v)); }

  static Mword prselr()
  { Mword v; asm volatile("mrc p15, 0, %0, c6, c2, 1" : "=r"(v)); return v; }

  static void prselr(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c2, 1" : : "r"(v)); }

  static void prenr_mask(Mword mask)
  {
    // This hurts. There is no PRENR register available...
    #define UPDATE(i) \
      do \
        { \
          if (i < Mem_layout::Mpu_regions && !(mask & (1UL << (i)))) \
            Mpu_arm_el1::prlar##i(0); \
        } \
      while (false)

    // Directly skip non-existing regions. We don't support more than 32 regions.
    static_assert(Mem_layout::Mpu_regions <= 32, "No more than 32 regions!");
    switch (Mpu_arm_el1::regions())
      {
        default:
        case 32: UPDATE(31); [[fallthrough]];
        case 31: UPDATE(30); [[fallthrough]];
        case 30: UPDATE(29); [[fallthrough]];
        case 29: UPDATE(28); [[fallthrough]];
        case 28: UPDATE(27); [[fallthrough]];
        case 27: UPDATE(26); [[fallthrough]];
        case 26: UPDATE(25); [[fallthrough]];
        case 25: UPDATE(24); [[fallthrough]];
        case 24: UPDATE(23); [[fallthrough]];
        case 23: UPDATE(22); [[fallthrough]];
        case 22: UPDATE(21); [[fallthrough]];
        case 21: UPDATE(20); [[fallthrough]];
        case 20: UPDATE(19); [[fallthrough]];
        case 19: UPDATE(18); [[fallthrough]];
        case 18: UPDATE(17); [[fallthrough]];
        case 17: UPDATE(16); [[fallthrough]];
        case 16: UPDATE(15); [[fallthrough]];
        case 15: UPDATE(14); [[fallthrough]];
        case 14: UPDATE(13); [[fallthrough]];
        case 13: UPDATE(12); [[fallthrough]];
        case 12: UPDATE(11); [[fallthrough]];
        case 11: UPDATE(10); [[fallthrough]];
        case 10: UPDATE(9);  [[fallthrough]];
        case  9: UPDATE(8);  [[fallthrough]];
        case  8: UPDATE(7);  [[fallthrough]];
        case  7: UPDATE(6);  [[fallthrough]];
        case  6: UPDATE(5);  [[fallthrough]];
        case  5: UPDATE(4);  [[fallthrough]];
        case  4: UPDATE(3);  [[fallthrough]];
        case  3: UPDATE(2);  [[fallthrough]];
        case  2: UPDATE(1);  [[fallthrough]];
        case  1: UPDATE(0);
          break;
      }

    #undef UPDATE
  }

  static Mword prbar0()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c8, 0" : "=r"(v)); return v; }
  static Mword prbar1()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c8, 4" : "=r"(v)); return v; }
  static Mword prbar2()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c9, 0" : "=r"(v)); return v; }
  static Mword prbar3()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c9, 4" : "=r"(v)); return v; }
  static Mword prbar4()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c10, 0" : "=r"(v)); return v; }
  static Mword prbar5()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c10, 4" : "=r"(v)); return v; }
  static Mword prbar6()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c11, 0" : "=r"(v)); return v; }
  static Mword prbar7()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c11, 4" : "=r"(v)); return v; }
  static Mword prbar8()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c12, 0" : "=r"(v)); return v; }
  static Mword prbar9()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c12, 4" : "=r"(v)); return v; }
  static Mword prbar10()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c13, 0" : "=r"(v)); return v; }
  static Mword prbar11()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c13, 4" : "=r"(v)); return v; }
  static Mword prbar12()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c14, 0" : "=r"(v)); return v; }
  static Mword prbar13()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c14, 4" : "=r"(v)); return v; }
  static Mword prbar14()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c15, 0" : "=r"(v)); return v; }
  static Mword prbar15()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c15, 4" : "=r"(v)); return v; }
  static Mword prbar16()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c8, 0" : "=r"(v)); return v; }
  static Mword prbar17()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c8, 4" : "=r"(v)); return v; }
  static Mword prbar18()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c9, 0" : "=r"(v)); return v; }
  static Mword prbar19()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c9, 4" : "=r"(v)); return v; }
  static Mword prbar20()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c10, 0" : "=r"(v)); return v; }
  static Mword prbar21()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c10, 4" : "=r"(v)); return v; }
  static Mword prbar22()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c11, 0" : "=r"(v)); return v; }
  static Mword prbar23()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c11, 4" : "=r"(v)); return v; }
  static Mword prbar24()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c12, 0" : "=r"(v)); return v; }
  static Mword prbar25()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c12, 4" : "=r"(v)); return v; }
  static Mword prbar26()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c13, 0" : "=r"(v)); return v; }
  static Mword prbar27()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c13, 4" : "=r"(v)); return v; }
  static Mword prbar28()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c14, 0" : "=r"(v)); return v; }
  static Mword prbar29()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c14, 4" : "=r"(v)); return v; }
  static Mword prbar30()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c15, 0" : "=r"(v)); return v; }
  static Mword prbar31()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c15, 4" : "=r"(v)); return v; }

  static Mword prlar0()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c8, 1" : "=r"(v)); return v; }
  static Mword prlar1()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c8, 5" : "=r"(v)); return v; }
  static Mword prlar2()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c9, 1" : "=r"(v)); return v; }
  static Mword prlar3()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c9, 5" : "=r"(v)); return v; }
  static Mword prlar4()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c10, 1" : "=r"(v)); return v; }
  static Mword prlar5()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c10, 5" : "=r"(v)); return v; }
  static Mword prlar6()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c11, 1" : "=r"(v)); return v; }
  static Mword prlar7()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c11, 5" : "=r"(v)); return v; }
  static Mword prlar8()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c12, 1" : "=r"(v)); return v; }
  static Mword prlar9()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c12, 5" : "=r"(v)); return v; }
  static Mword prlar10()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c13, 1" : "=r"(v)); return v; }
  static Mword prlar11()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c13, 5" : "=r"(v)); return v; }
  static Mword prlar12()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c14, 1" : "=r"(v)); return v; }
  static Mword prlar13()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c14, 5" : "=r"(v)); return v; }
  static Mword prlar14()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c15, 1" : "=r"(v)); return v; }
  static Mword prlar15()
  { Mword v; asm volatile ("mrc p15, 0, %0, c6, c15, 5" : "=r"(v)); return v; }
  static Mword prlar16()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c8, 1" : "=r"(v)); return v; }
  static Mword prlar17()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c8, 5" : "=r"(v)); return v; }
  static Mword prlar18()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c9, 1" : "=r"(v)); return v; }
  static Mword prlar19()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c9, 5" : "=r"(v)); return v; }
  static Mword prlar20()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c10, 1" : "=r"(v)); return v; }
  static Mword prlar21()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c10, 5" : "=r"(v)); return v; }
  static Mword prlar22()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c11, 1" : "=r"(v)); return v; }
  static Mword prlar23()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c11, 5" : "=r"(v)); return v; }
  static Mword prlar24()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c12, 1" : "=r"(v)); return v; }
  static Mword prlar25()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c12, 5" : "=r"(v)); return v; }
  static Mword prlar26()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c13, 1" : "=r"(v)); return v; }
  static Mword prlar27()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c13, 5" : "=r"(v)); return v; }
  static Mword prlar28()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c14, 1" : "=r"(v)); return v; }
  static Mword prlar29()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c14, 5" : "=r"(v)); return v; }
  static Mword prlar30()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c15, 1" : "=r"(v)); return v; }
  static Mword prlar31()
  { Mword v; asm volatile ("mrc p15, 1, %0, c6, c15, 5" : "=r"(v)); return v; }


  static void prxar0(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6,  c8, 0\n"
                 "mcr p15, 0, %1, c6,  c8, 1" : : "r"(b), "r"(l));
  }
  static void prxar1(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6,  c8, 4\n"
                 "mcr p15, 0, %1, c6 , c8, 5" : : "r"(b), "r"(l));
  }
  static void prxar2(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6,  c9, 0\n"
                 "mcr p15, 0, %1, c6,  c9, 1" : : "r"(b), "r"(l));
  }
  static void prxar3(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6,  c9, 4\n"
                 "mcr p15, 0, %1, c6,  c9, 5" : : "r"(b), "r"(l));
  }
  static void prxar4(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c10, 0\n"
                 "mcr p15, 0, %1, c6, c10, 1" : : "r"(b), "r"(l));
  }
  static void prxar5(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c10, 4\n"
                 "mcr p15, 0, %1, c6, c10, 5" : : "r"(b), "r"(l));
  }
  static void prxar6(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c11, 0\n"
                 "mcr p15, 0, %1, c6, c11, 1" : : "r"(b), "r"(l));
  }
  static void prxar7(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c11, 4\n"
                 "mcr p15, 0, %1, c6, c11, 5" : : "r"(b), "r"(l));
  }
  static void prxar8(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c12, 0\n"
                 "mcr p15, 0, %1, c6, c12, 1" : : "r"(b), "r"(l));
  }
  static void prxar9(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c12, 4\n"
                 "mcr p15, 0, %1, c6, c12, 5" : : "r"(b), "r"(l));
  }
  static void prxar10(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c13, 0\n"
                 "mcr p15, 0, %1, c6, c13, 1" : : "r"(b), "r"(l));
  }
  static void prxar11(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c13, 4\n"
                 "mcr p15, 0, %1, c6, c13, 5" : : "r"(b), "r"(l));
  }
  static void prxar12(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c14, 0\n"
                 "mcr p15, 0, %1, c6, c14, 1" : : "r"(b), "r"(l));
  }
  static void prxar13(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c14, 4\n"
                 "mcr p15, 0, %1, c6, c14, 5" : : "r"(b), "r"(l));
  }
  static void prxar14(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c15, 0\n"
                 "mcr p15, 0, %1, c6, c15, 1" : : "r"(b), "r"(l));
  }
  static void prxar15(Mword b, Mword l)
  {
    asm volatile("mcr p15, 0, %0, c6, c15, 4\n"
                 "mcr p15, 0, %1, c6, c15, 5" : : "r"(b), "r"(l));
  }
  static void prxar16(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6,  c8, 0\n"
                 "mcr p15, 1, %1, c6,  c8, 1" : : "r"(b), "r"(l));
  }
  static void prxar17(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6,  c8, 4\n"
                 "mcr p15, 1, %1, c6,  c8, 5" : : "r"(b), "r"(l));
  }
  static void prxar18(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6,  c9, 0\n"
                 "mcr p15, 1, %1, c6,  c9, 1" : : "r"(b), "r"(l));
  }
  static void prxar19(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6,  c9, 4\n"
                 "mcr p15, 1, %1, c6,  c9, 5" : : "r"(b), "r"(l));
  }
  static void prxar20(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c10, 0\n"
                 "mcr p15, 1, %1, c6, c10, 1" : : "r"(b), "r"(l));
  }
  static void prxar21(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c10, 4\n"
                 "mcr p15, 1, %1, c6, c10, 5" : : "r"(b), "r"(l));
  }
  static void prxar22(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c11, 0\n"
                 "mcr p15, 1, %1, c6, c11, 1" : : "r"(b), "r"(l));
  }
  static void prxar23(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c11, 4\n"
                 "mcr p15, 1, %1, c6, c11, 5" : : "r"(b), "r"(l));
  }
  static void prxar24(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c12, 0\n"
                 "mcr p15, 1, %1, c6, c12, 1" : : "r"(b), "r"(l));
  }
  static void prxar25(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c12, 4\n"
                 "mcr p15, 1, %1, c6, c12, 5" : : "r"(b), "r"(l));
  }
  static void prxar26(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c13, 0\n"
                 "mcr p15, 1, %1, c6, c13, 1" : : "r"(b), "r"(l));
  }
  static void prxar27(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c13, 4\n"
                 "mcr p15, 1, %1, c6, c13, 5" : : "r"(b), "r"(l));
  }
  static void prxar28(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c14, 0\n"
                 "mcr p15, 1, %1, c6, c14, 1" : : "r"(b), "r"(l));
  }
  static void prxar29(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c14, 4\n"
                 "mcr p15, 1, %1, c6, c14, 5" : : "r"(b), "r"(l));
  }
  static void prxar30(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c15, 0\n"
                 "mcr p15, 1, %1, c6, c15, 1" : : "r"(b), "r"(l));
  }
  static void prxar31(Mword b, Mword l)
  {
    asm volatile("mcr p15, 1, %0, c6, c15, 4\n"
                 "mcr p15, 1, %1, c6, c15, 5" : : "r"(b), "r"(l));
  }

  // We need dedicated PRLARx access for prenr_mask()

  static void prlar0(Mword v)
  { asm volatile("mcr p15, 0, %0, c6,  c8, 1" : : "r"(v)); }
  static void prlar1(Mword v)
  { asm volatile("mcr p15, 0, %0, c6 , c8, 5" : : "r"(v)); }
  static void prlar2(Mword v)
  { asm volatile("mcr p15, 0, %0, c6,  c9, 1" : : "r"(v)); }
  static void prlar3(Mword v)
  { asm volatile("mcr p15, 0, %0, c6,  c9, 5" : : "r"(v)); }
  static void prlar4(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c10, 1" : : "r"(v)); }
  static void prlar5(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c10, 5" : : "r"(v)); }
  static void prlar6(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c11, 1" : : "r"(v)); }
  static void prlar7(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c11, 5" : : "r"(v)); }
  static void prlar8(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c12, 1" : : "r"(v)); }
  static void prlar9(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c12, 5" : : "r"(v)); }
  static void prlar10(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c13, 1" : : "r"(v)); }
  static void prlar11(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c13, 5" : : "r"(v)); }
  static void prlar12(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c14, 1" : : "r"(v)); }
  static void prlar13(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c14, 5" : : "r"(v)); }
  static void prlar14(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c15, 1" : : "r"(v)); }
  static void prlar15(Mword v)
  { asm volatile("mcr p15, 0, %0, c6, c15, 5" : : "r"(v)); }
  static void prlar16(Mword v)
  { asm volatile("mcr p15, 1, %0, c6,  c8, 1" : : "r"(v)); }
  static void prlar17(Mword v)
  { asm volatile("mcr p15, 1, %0, c6,  c8, 5" : : "r"(v)); }
  static void prlar18(Mword v)
  { asm volatile("mcr p15, 1, %0, c6,  c9, 1" : : "r"(v)); }
  static void prlar19(Mword v)
  { asm volatile("mcr p15, 1, %0, c6,  c9, 5" : : "r"(v)); }
  static void prlar20(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c10, 1" : : "r"(v)); }
  static void prlar21(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c10, 5" : : "r"(v)); }
  static void prlar22(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c11, 1" : : "r"(v)); }
  static void prlar23(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c11, 5" : : "r"(v)); }
  static void prlar24(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c12, 1" : : "r"(v)); }
  static void prlar25(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c12, 5" : : "r"(v)); }
  static void prlar26(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c13, 1" : : "r"(v)); }
  static void prlar27(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c13, 5" : : "r"(v)); }
  static void prlar28(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c14, 1" : : "r"(v)); }
  static void prlar29(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c14, 5" : : "r"(v)); }
  static void prlar30(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c15, 1" : : "r"(v)); }
  static void prlar31(Mword v)
  { asm volatile("mcr p15, 1, %0, c6, c15, 5" : : "r"(v)); }
};

struct Mpu_arm_el2
{
  static void init()
  {
    // Do not touch region 0. We might execute from it!
    asm volatile("mcr p15, 4, %0, c6, c1, 1" : : "r"(1)); // HPRENR
  }

  static Mword regions()
  {
    Mword v;
    asm volatile("mrc p15, 4, %0, c0, c0, 4" : "=r"(v)); // HMPUIR
    return v;
  }

  static void prbar(Mword v)
  { asm volatile("mcr p15, 4, %0, c6, c3, 0" : : "r"(v)); }

  static Mword prlar()
  {
    Mword v;
    asm volatile("mrc p15, 4, %0, c6, c3, 1" : "=r"(v));
    return v;
  }

  static void prlar(Mword v)
  { asm volatile("mcr p15, 4, %0, c6, c3, 1" : : "r"(v)); }

  static void prselr(Mword v)
  { asm volatile("mcr p15, 4, %0, c6, c2, 1" : : "r"(v)); }

  static Mword prenr()
  {
    Mword v;
    asm volatile("mrc p15, 4, %0, c6, c1, 1" : "=r"(v));
    return v;
  }

  static void prenr(Mword v)
  { asm volatile("mcr p15, 4, %0, c6, c1, 1" : : "r"(v)); }

  static void prenr_mask(Mword mask)
  { prenr(Mpu_arm_el2::prenr() & mask); }


  static void prxar0(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6,  c8, 0\n"
                 "mcr p15, 4, %1, c6,  c8, 1" : : "r"(b), "r"(l));
  }
  static void prxar1(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6,  c8, 4\n"
                 "mcr p15, 4, %1, c6,  c8, 5" : : "r"(b), "r"(l));
  }
  static void prxar2(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6,  c9, 0\n"
                 "mcr p15, 4, %1, c6,  c9, 1" : : "r"(b), "r"(l));
  }
  static void prxar3(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6,  c9, 4\n"
                 "mcr p15, 4, %1, c6,  c9, 5" : : "r"(b), "r"(l));
  }
  static void prxar4(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c10, 0\n"
                 "mcr p15, 4, %1, c6, c10, 1" : : "r"(b), "r"(l));
  }
  static void prxar5(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c10, 4\n"
                 "mcr p15, 4, %1, c6, c10, 5" : : "r"(b), "r"(l));
  }
  static void prxar6(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c11, 0\n"
                 "mcr p15, 4, %1, c6, c11, 1" : : "r"(b), "r"(l));
  }
  static void prxar7(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c11, 4\n"
                 "mcr p15, 4, %1, c6, c11, 5" : : "r"(b), "r"(l));
  }
  static void prxar8(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c12, 0\n"
                 "mcr p15, 4, %1, c6, c12, 1" : : "r"(b), "r"(l));
  }
  static void prxar9(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c12, 4\n"
                 "mcr p15, 4, %1, c6, c12, 5" : : "r"(b), "r"(l));
  }
  static void prxar10(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c13, 0\n"
                 "mcr p15, 4, %1, c6, c13, 1" : : "r"(b), "r"(l));
  }
  static void prxar11(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c13, 4\n"
                 "mcr p15, 4, %1, c6, c13, 5" : : "r"(b), "r"(l));
  }
  static void prxar12(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c14, 0\n"
                 "mcr p15, 4, %1, c6, c14, 1" : : "r"(b), "r"(l));
  }
  static void prxar13(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c14, 4\n"
                 "mcr p15, 4, %1, c6, c14, 5" : : "r"(b), "r"(l));
  }
  static void prxar14(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c15, 0\n"
                 "mcr p15, 4, %1, c6, c15, 1" : : "r"(b), "r"(l));
  }
  static void prxar15(Mword b, Mword l)
  {
    asm volatile("mcr p15, 4, %0, c6, c15, 4\n"
                 "mcr p15, 4, %1, c6, c15, 5" : : "r"(b), "r"(l));
  }
  static void prxar16(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6,  c8, 0\n"
                 "mcr p15, 5, %1, c6,  c8, 1" : : "r"(b), "r"(l));
  }
  static void prxar17(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6,  c8, 4\n"
                 "mcr p15, 5, %1, c6,  c8, 5" : : "r"(b), "r"(l));
  }
  static void prxar18(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6,  c9, 0\n"
                 "mcr p15, 5, %1, c6,  c9, 1" : : "r"(b), "r"(l));
  }
  static void prxar19(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6,  c9, 4\n"
                 "mcr p15, 5, %1, c6,  c9, 5" : : "r"(b), "r"(l));
  }
  static void prxar20(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c10, 0\n"
                 "mcr p15, 5, %1, c6, c10, 1" : : "r"(b), "r"(l));
  }
  static void prxar21(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c10, 4\n"
                 "mcr p15, 5, %1, c6, c10, 5" : : "r"(b), "r"(l));
  }
  static void prxar22(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c11, 0\n"
                 "mcr p15, 5, %1, c6, c11, 1" : : "r"(b), "r"(l));
  }
  static void prxar23(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c11, 4\n"
                 "mcr p15, 5, %1, c6, c11, 5" : : "r"(b), "r"(l));
  }
  static void prxar24(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c12, 0\n"
                 "mcr p15, 5, %1, c6, c12, 1" : : "r"(b), "r"(l));
  }
  static void prxar25(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c12, 4\n"
                 "mcr p15, 5, %1, c6, c12, 5" : : "r"(b), "r"(l));
  }
  static void prxar26(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c13, 0\n"
                 "mcr p15, 5, %1, c6, c13, 1" : : "r"(b), "r"(l));
  }
  static void prxar27(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c13, 4\n"
                 "mcr p15, 5, %1, c6, c13, 5" : : "r"(b), "r"(l));
  }
  static void prxar28(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c14, 0\n"
                 "mcr p15, 5, %1, c6, c14, 1" : : "r"(b), "r"(l));
  }
  static void prxar29(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c14, 4\n"
                 "mcr p15, 5, %1, c6, c14, 5" : : "r"(b), "r"(l));
  }
  static void prxar30(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c15, 0\n"
                 "mcr p15, 5, %1, c6, c15, 1" : : "r"(b), "r"(l));
  }
  static void prxar31(Mword b, Mword l)
  {
    asm volatile("mcr p15, 5, %0, c6, c15, 4\n"
                 "mcr p15, 5, %1, c6, c15, 5" : : "r"(b), "r"(l));
  }

};

//------------------------------------------------------------------
INTERFACE [arm && 32bit && mpu && arm_v8 && !cpu_virt]:

typedef Mpu_arm_el1 Mpu_arm;

//------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && mpu && arm_v8 && !cpu_virt]:

PUBLIC static
bool
Mpu::enabled()
{
  unsigned control;
  asm("mrc p15, 0, %0, c1, c0" : "=r"(control)); // SCTLR
  return control & 1U; // SCTLR.M
}

IMPLEMENT static
void Mpu::init()
{
  Mpu_arm::init();
  asm volatile ("mcr p15, 0, %0, c10, c2, 0" : : "r"(Mpu::Mair0_bits));
  asm volatile ("mcr p15, 0, %0, c10, c2, 1" : : "r"(Mpu::Mair1_bits));
}

//------------------------------------------------------------------
INTERFACE [arm && 32bit && mpu && arm_v8 && cpu_virt]:

typedef Mpu_arm_el2 Mpu_arm;

//------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && mpu && arm_v8 && cpu_virt]:

PUBLIC static
bool
Mpu::enabled()
{
  unsigned control;
  asm("mrc p15, 4, %0, c1, c0" : "=r"(control)); // HSCTLR
  return control & 1U; // HSCTLR.M
}

IMPLEMENT static
void Mpu::init()
{
  Mpu_arm::init();
  asm volatile ("mcr p15, 4, %0, c10, c2, 0" : : "r"(Mpu::Mair0_bits));
  asm volatile ("mcr p15, 4, %0, c10, c2, 1" : : "r"(Mpu::Mair1_bits));
}

//------------------------------------------------------------------
INTERFACE [arm && 32bit && mpu && arm_v8]:

EXTENSION struct Mpu_region
{
public:
  Mword prbar, prlar;

  struct Prot {
    enum : Mword {
      None = 0,
      NX   = 1UL << 0,
      EL0  = 1UL << 1,
      RO   = 1UL << 2,
    };
  };

  struct Attr {
    enum : Mword {
      Mask      = 7UL << 1,
      Normal    = 2UL << 1,
      Device    = 0UL << 1,
      Buffered  = 1UL << 1,
    };
  };

  enum : Mword { Disabled = 0, Enabled = 1 };
};


EXTENSION class Mpu
{
public:
  enum : Mword {
    /**
     * Memory Attribute Indirection (MAIR0)
     * Attr0: Device-nGnRnE memory
     * Attr1: Normal memory, Inner/Outer Non-cacheable
     * Attr2: Normal memory, Read-Allocate, no Write-Allocate,
     *        Inner/Outer Write-Through Cacheable (Non-transient)
     * Attr3: Device-nGnRnE memory (unused)
     */
    Mair0_bits = 0x00aa4400,
    /**
     * Memory Attribute Indirection (MAIR1)
     * Attr4..Attr7: Device-nGnRnE memory (unused)
     */
    Mair1_bits = 0,
  };
};

//------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit && mpu && arm_v8]:

IMPLEMENT constexpr
Mpu_region::Mpu_region()
: prbar(~0x3fUL), prlar(0)
{}

IMPLEMENT inline
Mpu_region::Mpu_region(Mword start, Mword end, Mpu_region_attr a)
: prbar(start & ~0x3fUL), prlar(end & ~0x3fUL)
{ attr(a); }

IMPLEMENT constexpr
Mword
Mpu_region::start() const
{ return prbar & ~0x3fUL; }

IMPLEMENT constexpr
Mword
Mpu_region::end() const
{ return prlar |  0x3fUL; }

IMPLEMENT constexpr
Mpu_region_attr
Mpu_region::attr() const
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
             ? L4_snd_item::Memory_type::Normal()
             : (((prlar & Attr::Mask) == Attr::Device)
                ? L4_snd_item::Memory_type::Uncached()
                : L4_snd_item::Memory_type::Buffered()),
            prlar & Enabled);
}

IMPLEMENT inline
void
Mpu_region::start(Mword start)
{
  prbar = (prbar & 0x3fUL) | (start & ~0x3fUL);
}

IMPLEMENT inline
void
Mpu_region::end(Mword end)
{
  prlar = (prlar & 0x3fUL) | (end & ~0x3fUL);
}

IMPLEMENT inline
void
Mpu_region::attr(Mpu_region_attr attr)
{
  prbar = (prbar & ~0x3fUL)
          | ((attr.rights() & L4_fpage::Rights::U()) ? Prot::EL0 : Prot::None)
          | ((attr.rights() & L4_fpage::Rights::W()) ? Prot::None : Prot::RO)
          | ((attr.rights() & L4_fpage::Rights::X()) ? Prot::None : Prot::NX);
  prlar = (prlar & ~0x3fUL)
          | ((attr.type() == L4_snd_item::Memory_type::Uncached())
              ? Attr::Device
              : ((attr.type() == L4_snd_item::Memory_type::Buffered())
                  ? Attr::Buffered
                  : Attr::Normal))
          | (attr.enabled() ? Enabled : Disabled);
}

IMPLEMENT inline
void
Mpu_region::disable()
{
  prlar &= ~Enabled;
}


IMPLEMENT static inline
unsigned Mpu::regions()
{
  return Mpu_arm::regions();
}

IMPLEMENT static inline
void
Mpu::sync(Mpu_regions const &regions, Mpu_regions_mask const &touched,
          bool inplace)
{
  unsigned i = 0;
  while (i < touched.size() && (i = touched.ffs(i)))
    {
      Mpu_arm::prselr(i - 1);
      Mem::isb();
      if (!inplace && (Mpu_arm::prlar() & Mpu_region::Enabled))
        {
          // Always disable first! Otherwise a colliding region might
          // exist briefly after writing prbar!
          Mpu_arm::prlar(0);
          Mem::isb();
        }

      Mpu_arm::prbar(regions[i - 1].prbar);
      Mpu_arm::prlar(regions[i - 1].prlar);
    }
}

IMPLEMENT static inline
void
Mpu::update(Mpu_regions const &regions)
{
  Mpu_regions_mask reserved = regions.reserved();

  // Disable regions that we're updating. Otherwise there is the possiblity to
  // have an invalid, colliding region when prbar is updated and the current
  // prlar of the updated region is still enabled.
  Mpu_arm::prenr_mask(*reserved.raw());
  Mem::isb();

  static_assert(reserved.size() <= 32,
                "HPRENR register only covers <= 32 regions!");

#define UPDATE(i) \
  do \
    { \
      if (i < Mem_layout::Mpu_regions) \
        Mpu_arm::prxar##i(regions[(i)].prbar, regions[(i)].prlar); \
    } \
  while (false)

  // Directly skip non-existing regions. We don't support more than 32 regions.
  static_assert(Mem_layout::Mpu_regions <= 32, "No more than 32 regions!");
  switch (regions.size())
    {
      default:
      case 32: UPDATE(31); [[fallthrough]];
      case 31: UPDATE(30); [[fallthrough]];
      case 30: UPDATE(29); [[fallthrough]];
      case 29: UPDATE(28); [[fallthrough]];
      case 28: UPDATE(27); [[fallthrough]];
      case 27: UPDATE(26); [[fallthrough]];
      case 26: UPDATE(25); [[fallthrough]];
      case 25: UPDATE(24); [[fallthrough]];
      case 24: UPDATE(23); [[fallthrough]];
      case 23: UPDATE(22); [[fallthrough]];
      case 22: UPDATE(21); [[fallthrough]];
      case 21: UPDATE(20); [[fallthrough]];
      case 20: UPDATE(19); [[fallthrough]];
      case 19: UPDATE(18); [[fallthrough]];
      case 18: UPDATE(17); [[fallthrough]];
      case 17: UPDATE(16); [[fallthrough]];
      case 16: UPDATE(15); [[fallthrough]];
      case 15: UPDATE(14); [[fallthrough]];
      case 14: UPDATE(13); [[fallthrough]];
      case 13: UPDATE(12); [[fallthrough]];
      case 12: UPDATE(11); [[fallthrough]];
      case 11: UPDATE(10); [[fallthrough]];
      case 10: UPDATE(9);  [[fallthrough]];
      case  9: UPDATE(8);  [[fallthrough]];
      case  8: UPDATE(7);  [[fallthrough]];
      case  7: UPDATE(6);  [[fallthrough]];
      case  6: UPDATE(5);  [[fallthrough]];
      case  5: UPDATE(4);  [[fallthrough]];
      case  4: UPDATE(3);
        // UPDATE(2) // HEAP
        // UPDATE(1) // KIP
        // UPDATE(0) // kernel text
        break;
    }

#undef UPDATE

  // Theoretically, because only user space regions are reconfigured, the ERET
  // on the kernel exit should be sufficient. But there we read/modify/write
  // PRENR. Hence, all region updates must be already committed to not read
  // stale data through PRENR.
  Mem::isb();
}
