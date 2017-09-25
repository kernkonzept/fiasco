INTERFACE [arm && perf_cnt]:

#include "initcalls.h"

EXTENSION class Perf_cnt
{
public:
  static Perf_read_fn read_pmc[Max_slot];

  static const char *perf_type_str;
};

// ------------------------------------------------------------------------
INTERFACE [arm && perf_cnt && !(mpcore || armca8 || armca9)]:

EXTENSION class Perf_cnt
{
private:
  enum
  {
    Nr_of_events = 0,
  };

};

// ------------------------------------------------------------------------
INTERFACE [arm && perf_cnt && mpcore]:

#include "mem_layout.h"

EXTENSION class Perf_cnt
{
private:
  enum {
    CPU_CONTROL = 0x00,
    CONFIG      = 0x04,
    CPU_STATUS  = 0x08,
    MON_CONTROL = 0x10,

    MON_CONTROL_ENABLE = 1,
    MON_CONTROL_RESET  = 2,

    EVENT_DISABLED              = 0,
    // data not available in any other CPU
    EVENT_EXTMEM_CPU0 = 1,
    EVENT_EXTMEM_CPU1 = 2,
    EVENT_EXTMEM_CPU2 = 3,
    EVENT_EXTMEM_CPU3 = 4,
    // data available in another CPU cache
    EVENT_OTHER_CACHE_HIT_CPU0 = 5,
    EVENT_OTHER_CACHE_HIT_CPU1 = 6,
    EVENT_OTHER_CACHE_HIT_CPU2 = 7,
    EVENT_OTHER_CACHE_HIT_CPU3 = 8,
    // non-present
    EVENT_NON_PRESENT_CPU0 = 9,
    EVENT_NON_PRESENT_CPU1 = 10,
    EVENT_NON_PRESENT_CPU2 = 11,
    EVENT_NON_PRESENT_CPU3 = 12,

    // line migration instead of sharing
    EVENT_LINE_MIGRATION = 13,

    // memory
    EVENT_READ_BUSY_MASTER0     = 14,
    EVENT_READ_BUSY_MASTER1     = 15,
    EVENT_WRITE_BUSY_MASTER0    = 16,
    EVENT_WRITE_BUSY_MASTER1    = 17,
    EVENT_EXTMEM_TRANSFER_READ  = 18,
    EVENT_EXTMEM_TRANSFER_WRITE = 19,

    EVENT_CYCLE_COUNT = 31,

    Nr_of_events = 32,
  };

  static Address mon_event_type_addr(int nr)
  { return 0x14 + nr; }

  static Address mon_counter(int nr)
  { return 0x1c + nr * 4; }
};

// ------------------------------------------------------------------------
INTERFACE [arm && perf_cnt && (armca8 || armca9)]:

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
INTERFACE [arm && perf_cnt && armca8]:

EXTENSION class Perf_cnt
{
private:
  enum
  {
    Nr_of_events = 0x73,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && perf_cnt && armca9]:

EXTENSION class Perf_cnt
{
private:
  enum
  {
    Nr_of_events = 0x94,
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt]:

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt && !(mpcore || armca8 || armca9)]:

char const *Perf_cnt::perf_type_str = "none";

PUBLIC static FIASCO_INIT_CPU
void
Perf_cnt::init_cpu()
{}

PUBLIC static inline
Unsigned64
Perf_cnt::read_cycle_cnt()
{ return 0; }

PUBLIC static inline
unsigned
Perf_cnt::mon_event_type(int)
{ return 0; }

PUBLIC static inline
unsigned long
Perf_cnt::read_counter(int)
{ return 0; }

PRIVATE static inline
void
Perf_cnt::set_event_type(int, int)
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt && mpcore]:

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

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt && (armca8 || armca9)]:

#include "cpu.h"

char const *Perf_cnt::perf_type_str = "ACor";
int Perf_cnt::_nr_counters;

PRIVATE static
bool
Perf_cnt::is_avail()
{
  switch (Cpu::boot_cpu()->copro_dbg_model())
    {
      case Cpu::Copro_dbg_model_v7:
      case Cpu::Copro_dbg_model_v7_1: return true;
      default: return false;
    }
}

PRIVATE static
void
Perf_cnt::set_event_type(int counter_nr, int event)
{
  if (!is_avail())
    return;

  pmnxsel(counter_nr);
  evtsel(event);
}

PUBLIC static
Unsigned64
Perf_cnt::read_cycle_cnt()
{
  if (!is_avail())
    return 0;
  return ccnt();
}

PUBLIC static
unsigned long
Perf_cnt::read_counter(int counter_nr)
{
  if (!is_avail())
    return 0;
  if (counter_nr >= _nr_counters)
    return ccnt();
  pmnxsel(counter_nr);
  return pmcnt();
}

PUBLIC static
unsigned
Perf_cnt::mon_event_type(int nr)
{
  if (!is_avail())
    return 0;

  if (nr >= _nr_counters)
    return 0xff;
  pmnxsel(nr);
  return evtsel();
}

PUBLIC static FIASCO_INIT_CPU
void
Perf_cnt::init_cpu()
{
  if (!is_avail())
    return;

  _nr_counters = (pmcr() >> 11) & 0x1f;

  pmcr(PMNC_ENABLE | PMNC_PERF_RESET | PMNC_CNT_RESET);

  cntens((1 << 31) | ((1 << _nr_counters) - 1));

  //set_event_type(0, 8);

  // allow user to access events
  useren(1);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && perf_cnt]:

#include <cstdio>
#include "static_init.h"
#include "tb_entry.h"

Perf_cnt::Perf_read_fn Perf_cnt::read_pmc[Max_slot] =
{ dummy_read_pmc, dummy_read_pmc };

static Mword dummy_read_pmc() { return 0; }

PUBLIC static
Mword
Perf_cnt::read_counter_0()
{ return read_counter(0); }

PRIVATE static
Mword
Perf_cnt::read_counter_1()
{ return read_counter(1); }

PUBLIC static void
Perf_cnt::get_unit_mask(Mword, Unit_mask_type *type, Mword *, Mword *)
{
  *type = Perf_cnt::Fixed;
}


PUBLIC static void
Perf_cnt::get_unit_mask_entry(Mword, Mword, Mword *value, const char **desc)
{
  *value = 0;
  *desc  = 0;
}


PUBLIC static void
Perf_cnt::get_perf_event(Mword nr, unsigned *evntsel,
                         const char **name, const char **desc)
{
  // having one set of static strings in here should be ok
  static char _name[20];
  static char _desc[50];

  snprintf(_name, sizeof(_name), "Event_%lx", nr);
  _name[sizeof(_name) - 1] = 0;

  snprintf(_desc, sizeof(_desc), "Check manual for description of event %lx", nr);
  _desc[sizeof(_desc) - 1] = 0;

  *name = (const char *)&_name;
  *desc = (const char *)&_desc;
  *evntsel = nr;
}

PUBLIC static Mword
Perf_cnt::get_max_perf_event()
{
  return Nr_of_events;
}

PUBLIC static void
Perf_cnt::split_event(Mword event, unsigned *evntsel, Mword *)
{
  *evntsel = event;
}

PUBLIC static Mword
Perf_cnt::lookup_event(Mword) { return 0; }

PUBLIC static void
Perf_cnt::combine_event(Mword evntsel, Mword, Mword *event)
{
  *event = evntsel;
}

PUBLIC static char const *
Perf_cnt::perf_type() { return perf_type_str; }

STATIC_INITIALIZE_P(Perf_cnt, PERF_CNT_INIT_PRIO);

PUBLIC static FIASCO_INIT_CPU
void
Perf_cnt::init()
{
  init_cpu();

  read_pmc[0] = read_counter_0;
  read_pmc[1] = read_counter_1;

  Tb_entry::set_cycle_read_func(read_cycle_cnt);
}

PUBLIC static inline FIASCO_INIT_CPU
void
Perf_cnt::init_ap()
{
  init_cpu();
}

PUBLIC static int
Perf_cnt::mode(Mword slot, const char **mode, const char **name,
               Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  static char _n[Max_slot][5];

  if (slot >= Max_slot)
    return 0;

  *event = mon_event_type(slot);

    snprintf(_n[slot], sizeof(_n[slot]), "e%lx", *event);
  _n[slot][sizeof(_n[slot]) - 1] = 0;
  *name = _n[slot];

  *mode = "";
  *user = *kern = *edge = 0;

  return 1;
}

PUBLIC static
int
Perf_cnt::setup_pmc(Mword slot, Mword event, Mword, Mword, Mword)
{
  if (slot >= Max_slot)
    return 0;

  set_event_type(slot, event);

  Tb_entry::set_rdcnt(slot, read_pmc[slot]);

  return 1;
}
