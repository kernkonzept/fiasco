
INTERFACE [arm && perf_cnt && (arm_v7 || arm_v8)]:

EXTENSION class Perf_cnt
{
private:
  static void pmcr(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r" (val)); // PMCR
  }

  static Mword pmcr()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (val)); // PMCR
    return val;
  }


  static void cntens(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r" (val)); // PMCNTENSET
  }

  static Mword cntens()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c12, 1" : "=r" (val)); // PMCNTENSET
    return val;
  }


  static void cntenc(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c12, 2" : : "r" (val)); // PMCNTENCLR
  }

  static Mword cntenc()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c12, 2" : "=r" (val)); // PMCNTENCLR
    return val;
  }


  static void flag(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c12, 3" : : "r" (val)); // PMOVSR
  }

  static Mword flag()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c12, 3" : "=r" (val)); // PMOVSR
    return val;
  }

  static void pmnxsel(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c12, 5" : : "r" (val)); // PMSELR
  }

  static Mword pmnxsel()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c12, 5" : "=r" (val)); // PMSELR
    return val;
  }


  static void ccnt(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c13, 0" : : "r" (val)); // PMCCNTR
  }

  static Mword ccnt()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (val)); // PMCCNTR
    return val;
  }

  static void ccnt_init(Cpu const &cpu);


  static void evtsel(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c13, 1" : : "r" (val)); // PMXEVTYPER
  }

  static Mword evtsel()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c13, 1" : "=r" (val)); // PMXEVTYPER
    return val;
  }


  static void pmcnt(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c13, 2" : : "r" (val)); // PMXEVCNTR
  }

  static Mword pmcnt()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c13, 2" : "=r" (val)); // PMXEVCNTR
    return val;
  }


  static void useren(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c14, 0" : : "r" (val)); // PMUSERENR
  }

  static Mword useren()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c14, 0" : "=r" (val)); // PMUSERENR
    return val;
  }


  static void intens(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c14, 1" : : "r" (val)); // PMINTENSET
  }

  static Mword intens()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c14, 1" : "=r" (val)); // PMINTENSET
    return val;
  }

  static void intenc(Mword val)
  {
    asm volatile ("mcr p15, 0, %0, c9, c14, 2" : : "r" (val)); // PMINTENCLR
  }

  static Mword intenc()
  {
    Mword val;
    asm volatile ("mrc p15, 0, %0, c9, c14, 2" : "=r" (val)); // PMINTENCLR
    return val;
  }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt && arm_mpcore]:

#include "cpu.h"

char const *Perf_cnt::perf_type_str = "MP-C";

PRIVATE static
void
Perf_cnt::set_event_type(int counter_nr, int event)
{ Cpu::scu->write<unsigned char>(event, mon_event_type_addr(counter_nr)); }

PUBLIC static
unsigned long
Perf_cnt::read_counter(int counter_nr)
{ return Cpu::scu->read<Mword>(mon_counter(counter_nr)); }

PUBLIC static FIASCO_INIT_CPU
void
Perf_cnt::init_cpu(Cpu const &)
{
  static_assert(Scu::Available, "No SCU available in this configuration");

  Cpu::scu->write<Mword>(0xff << 16 // clear overflow flags
                        | MON_CONTROL_RESET | MON_CONTROL_ENABLE,
                        MON_CONTROL);

  // static config for now...
  set_event_type(7, EVENT_CYCLE_COUNT);

  //set_event_type(0, EVENT_EXTMEM_TRANSFER_READ);
  //set_event_type(1, EVENT_EXTMEM_TRANSFER_WRITE);

#if 0
  set_event_type(3, EVENT_EXTMEM_TRANSFER_READ);
  set_event_type(4, EVENT_LINE_MIGRATION);
  set_event_type(5, EVENT_EXTMEM_CPU2);
  set_event_type(6, EVENT_OTHER_CACHE_HIT_CPU2);
  set_event_type(7, EVENT_NON_PRESENT_CPU2);
#endif

  //set_event_type(3, EVENT_READ_BUSY_MASTER0);
  //set_event_type(4, EVENT_READ_BUSY_MASTER1);
  //set_event_type(5, EVENT_WRITE_BUSY_MASTER0);
  //set_event_type(6, EVENT_WRITE_BUSY_MASTER1);
}

PUBLIC static
Unsigned64
Perf_cnt::read_cycle_cnt()
{ return read_counter(7); }

PUBLIC static
unsigned
Perf_cnt::mon_event_type(int nr)
{ return Cpu::scu->read<unsigned char>(mon_event_type_addr(nr)); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt && (arm_v7 || arm_v8)]:

IMPLEMENT
void
Perf_cnt::ccnt_init(Cpu const &cpu)
{
  // Make sure that the cycle counters also count cycles in EL2.
  if (cpu.has_pmuv2())
    {
      pmnxsel(0x1f);
      Mem::isb();

      // PMXEVTYPER now alias for PMCCFILTR
      Mword val;
      asm volatile ("mrc p15, 0, %0, c9, c13, 1" : "=r" (val)); // PMXEVTYPER
      val &= ~(1UL << 31); //   P=0: don't disable counting of cycles in EL1
      val &= ~(1UL << 30); //   U=0: don't disable counting of cycles in EL0
      val &= ~(1UL << 29); // NSK=0: don't disable counting of cycles in
                           // non-secure EL1
      val &= ~(1UL << 28); // NSU=0: don't disable counting of cycles in
                           // non-secure EL0
      if constexpr (TAG_ENABLED(cpu_virt))
        val |= (1UL << 27); // NSH=1: count cycles in EL2
      asm volatile ("mcr p15, 0, %0, c9, c13, 1" :: "r" (val)); // PMXEVTYPER
    }
}
