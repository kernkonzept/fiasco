INTERFACE [arm && arm_v8]:

EXTENSION class Perf_cnt
{
private:
  static void pmcr(Mword val)
  { asm volatile ("msr PMCR_EL0, %0" : : "r" (val)); }

  static Mword pmcr()
  { Mword val; asm volatile ("mrs %0, PMCR_EL0" : "=r" (val)); return val;}


  static void cntens(Mword val)
  { asm volatile ("msr PMCNTENSET_EL0, %0" : : "r" (val)); }

  static Mword cntens()
  { Mword val; asm volatile ("mrs %0, PMCNTENSET_EL0" : "=r" (val)); return val;}


  static void cntenc(Mword val)
  { asm volatile ("msr PMCNTENCLR_EL0, %0" : : "r" (val)); }


  static void flag(Mword val)
  { asm volatile ("msr PMOVSCLR_EL0, %0" : : "r" (val)); }

  static Mword flag()
  { Mword val; asm volatile ("mrs %0, PMOVSCLR_EL0" : "=r" (val)); return val;}

  static void pmnxsel(Mword val)
  { asm volatile ("msr PMSELR_EL0, %0" : : "r" (val)); }

  static Mword pmnxsel()
  { Mword val; asm volatile ("mrs %0, PMSELR_EL0" : "=r" (val)); return val;}


  static void ccnt(Mword val)
  { asm volatile ("msr PMCCNTR_EL0, %0" : : "r" (val)); }

  static Mword ccnt()
  { Mword val; asm volatile ("mrs %0, PMCCNTR_EL0" : "=r" (val)); return val;}

  static void ccnt_init(Cpu const &cpu);


  static void evtsel(Mword val)
  { asm volatile ("msr PMXEVTYPER_EL0, %0" : : "r" (val)); }

  static Mword evtsel()
  { Mword val; asm volatile ("mrs %0, PMXEVTYPER_EL0" : "=r" (val)); return val;}


  static void pmcnt(Mword val)
  { asm volatile ("msr PMXEVCNTR_EL0, %0" : : "r" (val)); }

  static Mword pmcnt()
  { Mword val; asm volatile ("mrs %0, PMXEVCNTR_EL0" : "=r" (val)); return val;}


  static void useren(Mword val)
  { asm volatile ("msr PMUSERENR_EL0, %0" : : "r" (val)); }

  static Mword useren()
  { Mword val; asm volatile ("mrs %0, PMUSERENR_EL0" : "=r" (val)); return val;}


  static void intens(Mword val)
  { asm volatile ("msr PMINTENSET_EL1, %0" : : "r" (val)); }

  static Mword intens()
  { Mword val; asm volatile ("mrs %0, PMINTENSET_EL1" : "=r" (val)); return val;}

  static void intenc(Mword val)
  { asm volatile ("msr PMINTENCLR_EL1, %0" : : "r" (val)); }
};

// --------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

IMPLEMENT
void
Perf_cnt::ccnt_init(Cpu const &cpu)
{
  if (cpu.has_pmuv3())
    {
      Mword val;
      asm volatile ("mrs %0, PMCCFILTR_EL0" : "=r" (val));
      val &= ~(1UL << 31); //   P=0: don't disable counting of cycles in EL1
      val &= ~(1UL << 30); //   U=0: don't disable counting of cycles in EL0
      val &= ~(1UL << 29); // NSK=0: don't disable counting of cycles in
                           // non-secure EL1
      val &= ~(1UL << 28); // NSU=0: don't disable counting of cycles in
                           // non-secure EL0
      if constexpr (Proc::Is_hyp)
        val |= (1UL << 27); // NSH=1: don't disable counting of cycles in EL2
      asm volatile ("msr PMCCFILTR_EL0, %0" : : "r" (val));
    }
};
