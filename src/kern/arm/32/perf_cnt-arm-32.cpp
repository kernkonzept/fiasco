INTERFACE [arm && perf_cnt && (arm_v7 || arm_v8)]:

EXTENSION class Perf_cnt
{
private:
  static void pmcr(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r" (val)); }

  static Mword pmcr()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (val)); return val;}


  static void cntens(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r" (val)); }

  static Mword cntens()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 1" : "=r" (val)); return val;}


  static void cntenc(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 2" : : "r" (val)); }

  static Mword cntenc()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 2" : "=r" (val)); return val;}


  static void flag(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 3" : : "r" (val)); }

  static Mword flag()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 3" : "=r" (val)); return val;}

  static void pmnxsel(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c12, 5" : : "r" (val)); }

  static Mword pmnxsel()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c12, 5" : "=r" (val)); return val;}


  static void ccnt(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c13, 0" : : "r" (val)); }

  static Mword ccnt()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (val)); return val;}


  static void evtsel(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c13, 1" : : "r" (val)); }

  static Mword evtsel()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c13, 1" : "=r" (val)); return val;}


  static void pmcnt(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c13, 2" : : "r" (val)); }

  static Mword pmcnt()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c13, 2" : "=r" (val)); return val;}


  static void useren(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c14, 0" : : "r" (val)); }

  static Mword useren()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c14, 0" : "=r" (val)); return val;}


  static void intens(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c14, 1" : : "r" (val)); }

  static Mword intens()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c14, 1" : "=r" (val)); return val;}

  static void intenc(Mword val)
  { asm volatile ("mcr p15, 0, %0, c9, c14, 2" : : "r" (val)); }

  static Mword intenc()
  { Mword val; asm volatile ("mrc p15, 0, %0, c9, c14, 2" : "=r" (val)); return val;}

  enum
  {
    PMNC_ENABLE     = 1 << 0,
    PMNC_PERF_RESET = 1 << 1,
    PMNC_CNT_RESET  = 1 << 2,
  };


  static int _nr_counters;
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
Perf_cnt::init_cpu()
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

