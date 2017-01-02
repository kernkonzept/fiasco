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


  static void evtsel(Mword val)
  { asm volatile ("msr PMXEVCNTR_EL0, %0" : : "r" (val)); }

  static Mword evtsel()
  { Mword val; asm volatile ("mrs %0, PMXEVCNTR_EL0" : "=r" (val)); return val;}


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
  { asm volatile ("msr PMINTENCLR_E1, %0" : : "r" (val)); }

  enum
  {
    PMNC_ENABLE     = 1 << 0,
    PMNC_PERF_RESET = 1 << 1,
    PMNC_CNT_RESET  = 1 << 2,
  };


  static int _nr_counters;
};
