INTERFACE [{ia32,ux,amd64}-perf_cnt]:

#include "cpu.h"
#include "types.h"

EXTENSION class Perf_cnt
{
public:
  enum Perf_event_type
  { P5, P6, P4, };

  static Perf_read_fn read_pmc[Max_slot];
  virtual void init_loadcnt() = 0;
  virtual void start_pmc(Mword) = 0;

  static Perf_cnt_arch *pcnt;

private:
  static Perf_read_fn *read_pmc_fns;
  static Perf_read_fn read_pmc_fn[Max_slot];
  static char const *perf_type_str;
  static Perf_event_type perf_event_type;
};

class Perf_cnt_arch
{
public:
  // basic initialization
  virtual int  init() = 0;

  // set event the counter should count
  virtual void set_pmc_event(Mword slot) = 0;

  inline void touch_watchdog()
  { Cpu::wrmsr(hold_watchdog, _ctr_reg0 + pmc_watchdog); }

protected:
  Mword _nr_regs;
  Mword _sel_reg0;
  Mword _ctr_reg0;
  Mword _watchdog;

  struct Event
  {
    char  user;		// 1=count in user mode
    char  kern;		// 1=count in kernel mode
    char  edge;		// 1=count edge / 0=count duration
    Mword pmc;		// # of performance counter
    Mword bitmask;	// counter bitmask
    Mword evnt;		// event selector
  };

  static Mword    pmc_watchdog;                   // # perfcounter of watchdog
  static Mword    pmc_loadcnt;                    // # perfcounter of loadcnt
  static Signed64 hold_watchdog;
  static Event    pmc_event[Perf_cnt::Max_slot];  // index is slot number
  static char     pmc_alloc[Perf_cnt::Max_pmc];   // index is # of perfcounter
};

class Perf_cnt_p5 : public Perf_cnt_arch {};
class Perf_cnt_p6 : public Perf_cnt_arch {};
class Perf_cnt_k7 : public Perf_cnt_p6   {};
class Perf_cnt_p4 : public Perf_cnt_arch {};
class Perf_cnt_ap : public Perf_cnt_p6   {};

IMPLEMENTATION [{ia32,ux,amd64}-perf_cnt]:

#include <cstring>
#include <cstdio>
#include <panic.h>
#include "cpu.h"
#include "regdefs.h"
#include "static_init.h"
#include "tb_entry.h"

Perf_cnt::Perf_read_fn Perf_cnt::read_pmc[Perf_cnt::Max_slot] =
{ dummy_read_pmc, dummy_read_pmc };
Perf_cnt::Perf_read_fn Perf_cnt::read_pmc_fn[Perf_cnt::Max_slot] =
{ dummy_read_pmc, dummy_read_pmc };

Perf_cnt::Perf_read_fn *Perf_cnt::read_pmc_fns;
Perf_cnt::Perf_event_type Perf_cnt::perf_event_type;
Perf_cnt_arch *Perf_cnt::pcnt;
char const *Perf_cnt::perf_type_str = "n/a";

Mword Perf_cnt_arch::pmc_watchdog = (Mword)-1;
Mword Perf_cnt_arch::pmc_loadcnt  = (Mword)-1;
Signed64 Perf_cnt_arch::hold_watchdog;
Perf_cnt_arch::Event Perf_cnt_arch::pmc_event[Perf_cnt::Max_slot];
char  Perf_cnt_arch::pmc_alloc[Perf_cnt::Max_pmc];

static Perf_cnt_p5 perf_cnt_p5 __attribute__ ((init_priority(PERF_CNT_INIT_PRIO)));
static Perf_cnt_p6 perf_cnt_p6 __attribute__ ((init_priority(PERF_CNT_INIT_PRIO)));
static Perf_cnt_k7 perf_cnt_k7 __attribute__ ((init_priority(PERF_CNT_INIT_PRIO)));
static Perf_cnt_p4 perf_cnt_p4 __attribute__ ((init_priority(PERF_CNT_INIT_PRIO)));
static Perf_cnt_ap perf_cnt_ap __attribute__ ((init_priority(PERF_CNT_INIT_PRIO)));

enum
{
  Alloc_none            = 0,	// unallocated
  Alloc_perf            = 1,	// allocated as performance counter
  Alloc_watchdog        = 2,	// allocated for watchdog
};

enum
{
  // Intel P5
  Msr_p5_cesr		= 0x11,
  Msr_p5_ctr0		= 0x12,
  Msr_p5_ctr1		= 0x13,
  P5_evntsel_user	= 0x00000080,
  P5_evntsel_kern	= 0x00000040,
  P5_evntsel_duration	= 0x00000100,

  // Intel P6/PII/PIII
  Msr_p6_perfctr0	= 0xC1,
  Msr_p6_evntsel0	= 0x186,
  P6_evntsel_enable	= 0x00400000,
  P6_evntsel_int	= 0x00100000,
  P6_evntsel_user	= 0x00010000,
  P6_evntsel_kern	= 0x00020000,
  P6_evntsel_edge	= 0x00040000,

  // AMD K7/K8
  Msr_k7_evntsel0	= 0xC0010000,
  Msr_k7_perfctr0	= 0xC0010004,
  K7_evntsel_enable	= P6_evntsel_enable,
  K7_evntsel_int	= P6_evntsel_int,
  K7_evntsel_user	= P6_evntsel_user,
  K7_evntsel_kern	= P6_evntsel_kern,
  K7_evntsel_edge	= P6_evntsel_edge,

  // Intel P4
  Msr_p4_misc_enable	= 0x1A0,
  Msr_p4_perfctr0	= 0x300,
  Msr_p4_bpu_counter0	= 0x300,
  Msr_p4_cccr0		= 0x360,
  Msr_p4_fsb_escr0	= 0x3A2,
  P4_escr_user		= (1<<2),
  P4_escr_kern		= (1<<3),
  Msr_p4_bpu_cccr0	= 0x360,
  P4_cccr_ovf		= (1<<31),
  P4_cccr_ovf_pmi	= (1<<26),
  P4_cccr_complement	= (1<<19),
  P4_cccr_compare	= (1<<18),
  P4_cccr_required	= (3<<16),
  P4_cccr_enable	= (1<<12),

  Msr_ap_perfctr0       = 0xC1,
  Msr_ap_evntsel0       = 0x186,
  AP_evntsel_enable     = P6_evntsel_enable,
  AP_evntsel_int        = P6_evntsel_int,
  AP_evntsel_user       = P6_evntsel_user,
  AP_evntsel_kern       = P6_evntsel_kern,
  AP_evntsel_edge       = P6_evntsel_edge,
};

// -----------------------------------------------------------------------

enum
{
  Perfctr_x86_generic 		= 0,	/* any x86 with rdtsc */
  Perfctr_x86_intel_p5		= 1,	/* no rdpmc */
  Perfctr_x86_intel_p5mmx	= 2,
  Perfctr_x86_intel_p6		= 3,
  Perfctr_x86_intel_pii		= 4,
  Perfctr_x86_intel_piii	= 5,
  Perfctr_x86_intel_p4		= 11,	/* model 0 and 1 */
  Perfctr_x86_intel_p4m2	= 12,	/* model 2 */
  Perfctr_x86_intel_p4m3	= 16,	/* model 3 and above */
  Perfctr_x86_intel_pentm	= 14,
  Perfctr_x86_amd_k7		= 9,
  Perfctr_x86_amd_k8		= 13,
  Perfctr_x86_arch_perfmon      = 14,
};

enum perfctr_unit_mask_type
{
  perfctr_um_type_fixed,	/* one fixed (required) value */
  perfctr_um_type_exclusive,	/* exactly one of N values */
  perfctr_um_type_bitmask,	/* bitwise 'or' of N power-of-2 values */
};

struct perfctr_unit_mask_value
{
  unsigned int value;
  const char *description;	/* [NAME:]text */
};

struct perfctr_unit_mask
{
  unsigned int default_value;
  enum perfctr_unit_mask_type type:16;
  unsigned short nvalues;
  struct perfctr_unit_mask_value values[1/*nvalues*/];
};

struct perfctr_event
{
  unsigned int evntsel;
  unsigned int counters_set; /* P4 force this to be CPU-specific */
  const struct perfctr_unit_mask *unit_mask;
  const char *name;
  const char *description;
};

struct perfctr_event_set
{
  unsigned int cpu_type;
  const char *event_prefix;
  const struct perfctr_event_set *include;
  unsigned int nevents;
  const struct perfctr_event *events;
};

// The following functions are only available if the perfctr module
// is linked into the kernel. If not, all symbols perfctr_* are 0. */
//
// show all events
extern "C" void perfctr_set_cputype(unsigned)
  __attribute__((weak));
extern "C" const struct perfctr_event* perfctr_lookup_event(unsigned,
							    unsigned*)
  __attribute__((weak));
extern "C" const struct perfctr_event* perfctr_index_event(unsigned)
  __attribute__((weak));
extern "C" unsigned perfctr_get_max_event(void)
  __attribute__((weak));
extern "C" const struct perfctr_event_set *perfctr_cpu_event_set(unsigned)
  __attribute__((weak));

static inline
void
clear_msr_range(Mword base, Mword n)
{
  for (Mword i=0; i<n; i++)
    Cpu::wrmsr(0, base+i);
}

//--------------------------------------------------------------------
// dummy

static Mword dummy_read_pmc() { return 0; }

//--------------------------------------------------------------------
// Intel P5 (Pentium/Pentium MMX) has 2 performance counters. No overflow
// interrupt available. Some events are not symmtetric.
PUBLIC inline NOEXPORT
Perf_cnt_p5::Perf_cnt_p5()
  : Perf_cnt_arch(Msr_p5_cesr, Msr_p5_ctr0, 2, 0)
{}

FIASCO_INIT_CPU
int
Perf_cnt_p5::init()
{
  Cpu::wrmsr(0, Msr_p5_cesr); // disable both counters
  return 1;
}

void
Perf_cnt_p5::set_pmc_event(Mword slot)
{
  Unsigned64 msr;
  Mword event;
    
  event = pmc_event[slot].evnt;
  if (pmc_event[slot].user)
    event += P5_evntsel_user;
  if (pmc_event[slot].kern)
    event += P5_evntsel_kern;
  if (pmc_event[slot].kern)
    event += P5_evntsel_kern;
  if (!pmc_event[slot].edge)
    event += P5_evntsel_duration;

  msr = Cpu::rdmsr(Msr_p5_cesr);
  if (pmc_event[slot].pmc == 0)
    {
      msr &= 0xffff0000;
      msr |= event;
    }
  else
    {
      msr &= 0x0000ffff;
      msr |= (event << 16);
    }
  Cpu::wrmsr(msr, Msr_p5_cesr);
}

static Mword p5_read_pmc_0()
{ return Cpu::rdmsr(Msr_p5_ctr0); }
static Mword p5_read_pmc_1()
{ return Cpu::rdmsr(Msr_p5_ctr1); }

static Perf_cnt::Perf_read_fn p5_read_pmc_fns[] =
{ &p5_read_pmc_0, &p5_read_pmc_1 };


//--------------------------------------------------------------------
// Intel P6 (PPro/PII/PIII) has 2 performance counters. Overflow interrupt
// is available. Some events are not symmtetric.
PUBLIC inline NOEXPORT
Perf_cnt_p6::Perf_cnt_p6()
  : Perf_cnt_arch(Msr_p6_evntsel0, Msr_p6_perfctr0, 2, 1)
{}

PROTECTED
Perf_cnt_p6::Perf_cnt_p6(Mword sel_reg0, Mword ctr_reg0, 
      			 Mword nr_regs, Mword watchdog)
  : Perf_cnt_arch(sel_reg0, ctr_reg0, nr_regs, watchdog)
{}

FIASCO_INIT_CPU
int
Perf_cnt_p6::init()
{
  for (Mword i=0; i<_nr_regs; i++)
    {
      Cpu::wrmsr(0, _sel_reg0+i);
      Cpu::wrmsr(0, _ctr_reg0+i);
    }
  return 1;
}

void
Perf_cnt_p6::set_pmc_event(Mword slot)
{
  Mword event;

  event = pmc_event[slot].evnt;
  if (pmc_event[slot].user)
    event += P6_evntsel_user;
  if (pmc_event[slot].kern)
    event += P6_evntsel_kern;
  if (pmc_event[slot].edge)
    event += P6_evntsel_edge;

  // select event
  Cpu::wrmsr(event, _sel_reg0+pmc_event[slot].pmc);
}

void
Perf_cnt_p6::start_pmc(Mword /*reg_nr*/)
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(_sel_reg0);
  msr |= P6_evntsel_enable;  // enable both!! counters
  Cpu::wrmsr(msr, _sel_reg0);
}

void
Perf_cnt_p6::init_watchdog()
{
  Unsigned64 msr;

  msr = P6_evntsel_int  // Int enable: enable interrupt on overflow
      | P6_evntsel_kern // Monitor kernel-level events
      | P6_evntsel_user // Monitor user-level events
      | 0x79;           // #clocks CPU is not halted
  Cpu::wrmsr(msr, _sel_reg0+pmc_watchdog);
}

void
Perf_cnt_p6::init_loadcnt()
{
  Unsigned64 msr;

  msr = P6_evntsel_kern // Monitor kernel-level events
      | P6_evntsel_user // Monitor user-level events
      | 0x79;           // #clocks CPU is not halted
  Cpu::wrmsr(msr, _sel_reg0+pmc_loadcnt);

  printf("Load counter initialized (read with rdpmc(0x%02lX))\n", pmc_loadcnt);
}

void
Perf_cnt_p6::start_watchdog()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(_sel_reg0+pmc_watchdog);
  msr |= P6_evntsel_int; // Int enable: enable interrupt on overflow
  Cpu::wrmsr(msr, _sel_reg0+pmc_watchdog);
}

void
Perf_cnt_p6::stop_watchdog()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(_sel_reg0+pmc_watchdog);
  msr &= ~P6_evntsel_int; // Int enable: enable interrupt on overflow
  Cpu::wrmsr(msr, _sel_reg0+pmc_watchdog);
}

static Mword p6_read_pmc_0() { return Cpu::rdpmc(0, 0xC1); }
static Mword p6_read_pmc_1() { return Cpu::rdpmc(1, 0xC2); }

static Perf_cnt::Perf_read_fn p6_read_pmc_fns[] =
{ &p6_read_pmc_0, &p6_read_pmc_1 };


//--------------------------------------------------------------------
// AMD K7 (Athlon, K8=Athlon64) has 4 performance counters. All events
// seem to be symmetric. Overflow interrupts available.
PUBLIC inline NOEXPORT
Perf_cnt_k7::Perf_cnt_k7()
  : Perf_cnt_p6(Msr_k7_evntsel0, Msr_k7_perfctr0, 4, 1)
{}

void
Perf_cnt_k7::start_pmc(Mword reg_nr)
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(_sel_reg0+reg_nr);
  msr |= K7_evntsel_enable;
  Cpu::wrmsr(msr, _sel_reg0+reg_nr);
}

void
Perf_cnt_k7::init_watchdog()
{
  Unsigned64 msr;

  msr = K7_evntsel_int  // Int enable: enable interrupt on overflow
      | K7_evntsel_kern // Monitor kernel-level events
      | K7_evntsel_user // Monitor user-level events
      | 0x76;           // #clocks CPU is running
  Cpu::wrmsr(msr, _sel_reg0+pmc_watchdog);
}

void
Perf_cnt_k7::init_loadcnt()
{
  Unsigned64 msr;

  msr = K7_evntsel_kern // Monitor kernel-level events
      | K7_evntsel_user // Monitor user-level events
      | 0x76;           // #clocks CPU is running
  Cpu::wrmsr(msr, _sel_reg0+pmc_loadcnt);

  printf("Load counter initialized (read with rdpmc(0x%02lX))\n", pmc_loadcnt);
}

static Mword k7_read_pmc_0() { return Cpu::rdpmc(0, 0xC0010004); }
static Mword k7_read_pmc_1() { return Cpu::rdpmc(1, 0xC0010005); }
static Mword k7_read_pmc_2() { return Cpu::rdpmc(2, 0xC0010006); }
static Mword k7_read_pmc_3() { return Cpu::rdpmc(3, 0xC0010007); }

static Perf_cnt::Perf_read_fn k7_read_pmc_fns[] =
{ &k7_read_pmc_0, &k7_read_pmc_1, &k7_read_pmc_2, &k7_read_pmc_3 };


//--------------------------------------------------------------------
// Arch Perfmon. Intel Core architecture
PUBLIC inline NOEXPORT
Perf_cnt_ap::Perf_cnt_ap()
  : Perf_cnt_p6(Msr_ap_evntsel0, Msr_ap_perfctr0, 2, 1)
{
  Unsigned32 eax, ebx, ecx;
  Cpu::boot_cpu()->arch_perfmon_info(&eax, &ebx, &ecx);
  _nr_regs = (eax & 0x0000ff00) >> 8;
}

void
Perf_cnt_ap::start_pmc(Mword reg_nr)
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(_sel_reg0 + reg_nr);
  msr |= AP_evntsel_enable;
  Cpu::wrmsr(msr, _sel_reg0 + reg_nr);
}

void
Perf_cnt_ap::init_watchdog()
{
  Unsigned64 msr;

  msr = AP_evntsel_int    // Int enable: enable interrupt on overflow
        | AP_evntsel_kern // Monitor kernel-level events
        | AP_evntsel_user // Monitor user-level events
        | 0x3C;           // #clocks CPU is running
  Cpu::wrmsr(msr, _sel_reg0 + pmc_watchdog);
}

void
Perf_cnt_ap::init_loadcnt()
{
  Unsigned64 msr;

  msr = AP_evntsel_kern   // Monitor kernel-level events
        | AP_evntsel_user // Monitor user-level events
        | 0x3C;           // #clocks CPU is running
  Cpu::wrmsr(msr, _sel_reg0 + pmc_loadcnt);

  printf("Load counter initialized (read with rdpmc(0x%02lX))\n", pmc_loadcnt);
}


//--------------------------------------------------------------------
// Intel P4
PUBLIC inline NOEXPORT
Perf_cnt_p4::Perf_cnt_p4()
  : Perf_cnt_arch(Msr_p4_bpu_cccr0, Msr_p4_bpu_counter0, 2, 1)
{}

static inline NOEXPORT
Mword
Perf_cnt_p4::escr_event_select(Mword n)
{ return n << 25; }

static inline NOEXPORT
Mword
Perf_cnt_p4::escr_event_mask(Mword n)
{ return n << 9; }

static inline NOEXPORT
Mword
Perf_cnt_p4::cccr_threshold(Mword n)
{ return n << 20; }

static inline NOEXPORT
Mword
Perf_cnt_p4::cccr_escr_select(Mword n)
{ return n << 13; }

FIASCO_INIT_CPU
int
Perf_cnt_p4::init()
{
  Unsigned32 misc_enable = Cpu::rdmsr(Msr_p4_misc_enable);

  // performance monitoring available?
  if (!(misc_enable & (1<<7)))
    return 0;

  // disable precise event based sampling
  if (!(misc_enable & (1<<12)))
    clear_msr_range(0x3F1, 2);

  // ensure sane state of performance counter registers
  clear_msr_range(0x3A0, 26);
  if (Cpu::boot_cpu()->model() <= 2)
    clear_msr_range(0x3BA, 2);
  clear_msr_range(0x3BC, 3);
  clear_msr_range(0x3C0, 6);
  clear_msr_range(0x3C8, 6);
  clear_msr_range(0x3E0, 2);
  clear_msr_range(Msr_p4_cccr0, 18);
  clear_msr_range(Msr_p4_perfctr0, 18);

  return 1;
}

void
Perf_cnt_p4::set_pmc_event(Mword /*slot*/)
{}

void
Perf_cnt_p4::start_pmc(Mword reg_nr)
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(Msr_p4_bpu_cccr0 + reg_nr);
  msr |= P4_cccr_enable;
  Cpu::wrmsr(msr, Msr_p4_bpu_cccr0 + reg_nr);
}

void
Perf_cnt_p4::init_watchdog()
{
  Unsigned64 msr;
  
  msr = escr_event_select(0x13) // global power events
      | escr_event_mask(1)      // the processor is active (non-halted)
      | P4_escr_kern            // Monitor kernel-level events
      | P4_escr_user;           // Monitor user-level events
  Cpu::wrmsr(msr, Msr_p4_fsb_escr0);

  msr = P4_cccr_ovf_pmi         // performance monitor interrupt on overflow
      | P4_cccr_required	// must be set
      | cccr_escr_select(6);    // select ESCR to select events to be counted
  Cpu::wrmsr(msr, Msr_p4_bpu_cccr0 + pmc_watchdog);
}

void
Perf_cnt_p4::init_loadcnt()
{
  Unsigned64 msr;
  
  msr = escr_event_select(0x13) // global power events
      | escr_event_mask(1)      // the processor is active (non-halted)
      | P4_escr_kern            // Monitor kernel-level events
      | P4_escr_user;           // Monitor user-level events
  Cpu::wrmsr(msr, Msr_p4_fsb_escr0);

  msr = P4_cccr_required	// must be set
      | cccr_escr_select(6);    // select ESCR to select events to be counted

  Cpu::wrmsr(msr, Msr_p4_bpu_cccr0 + pmc_loadcnt);

  printf("Load counter initialized (read with rdpmc(0x%02lX))\n", 
          pmc_loadcnt + 0);
}

void
Perf_cnt_p4::start_watchdog()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(Msr_p4_bpu_cccr0);
  msr |= P4_cccr_ovf_pmi | P4_cccr_enable; // Int enable, Ctr enable
  msr &= ~P4_cccr_ovf;                     // clear Overflow
  Cpu::wrmsr(msr, Msr_p4_bpu_cccr0 + pmc_watchdog);
}

void
Perf_cnt_p4::stop_watchdog()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(Msr_p4_bpu_cccr0);
  msr &= ~(P4_cccr_ovf_pmi | P4_cccr_enable); // Int disable, Ctr disable
  Cpu::wrmsr(msr, Msr_p4_bpu_cccr0 + pmc_watchdog);
}

static
Mword
p4_read_pmc() { return 0; }

static Perf_cnt::Perf_read_fn p4_read_pmc_fns[] = { &p4_read_pmc };


//--------------------------------------------------------------------

PROTECTED inline
Perf_cnt_arch::Perf_cnt_arch(Mword sel_reg0, Mword ctr_reg0, 
			     Mword nr_regs, Mword watchdog)
{
  _sel_reg0 = sel_reg0;
  _ctr_reg0 = ctr_reg0;
  _nr_regs  = nr_regs;
  _watchdog = watchdog;

  for (Mword slot=0; slot<Perf_cnt::Max_slot; slot++)
    {
      pmc_event[slot].pmc  = (Mword)-1;
      pmc_event[slot].edge = 0;
    }
}

PUBLIC inline
Mword
Perf_cnt_arch::watchdog_allocated()
{ return (pmc_watchdog != (Mword)-1); }

PUBLIC inline
Mword
Perf_cnt_arch::loadcnt_allocated()
{ return (pmc_loadcnt != (Mword)-1); }

void
Perf_cnt_arch::alloc_watchdog()
{
  if (!_watchdog)
    panic("Watchdog not available");

  for (Mword pmc=0; pmc<_nr_regs; pmc++)
    if (pmc_alloc[pmc] == Alloc_none)
      {
	pmc_alloc[pmc] = Alloc_watchdog;
	pmc_watchdog   = pmc;
	return;
      }

  panic("No performance counter available for watchdog");
}

void
Perf_cnt_arch::alloc_loadcnt()
{
  if (pmc_alloc[0] == Alloc_watchdog)
    {
      // allocate the watchdog counter
      pmc_alloc[0] = Alloc_perf;
      pmc_loadcnt  = 0;
      // move the watchdog to another counter
      pmc_watchdog = (Mword)-1;
      alloc_watchdog();
      return;
    }

  if (pmc_alloc[0] == Alloc_none)
    {
      pmc_alloc[0] = Alloc_perf;
      pmc_loadcnt  = 0;
      return;
    }

  panic("No performance counter available for loadcounter");
}

// allocate a performance counter according to bitmask (some events depend
// on specific counters)
PRIVATE
int
Perf_cnt_arch::alloc_pmc(Mword slot, Mword bitmask)
{
  // free previous allocated counter
  Mword pmc = pmc_event[slot].pmc;
  if (pmc != (Mword)-1 && pmc_alloc[pmc] == Alloc_perf)
    {
      pmc_event[slot].pmc = (Mword)-1;
      pmc_alloc[pmc]      = Alloc_none;
    }

  // search counter according to bitmask
  for (pmc=0; pmc<_nr_regs; pmc++)
    if ((pmc_alloc[pmc] == Alloc_none) && (bitmask & (1<<pmc)))
      {
	pmc_event[slot].pmc = pmc;
	pmc_alloc[pmc]      = Alloc_perf;
	Perf_cnt::set_pmc_fn(slot, pmc);
	return 1;
      }

  // did not found an appropriate free counter (restricted by bitmask) so try
  // to re-assign the watchdog because the watchdog usually uses a more general
  // counter with no restrictions
  if (watchdog_allocated() && (bitmask & (1<<pmc_watchdog)))
    {
      // allocate the watchdog counter
      pmc_event[slot].pmc     = pmc_watchdog;
      pmc_alloc[pmc_watchdog] = Alloc_perf;
      Perf_cnt::set_pmc_fn(slot, pmc_watchdog);
      // move the watchdog to another counter
      pmc_watchdog            = (Mword)-1;
      alloc_watchdog();
      return 1;
    }

  return 0;
}

PUBLIC virtual
void
Perf_cnt_arch::clear_pmc(Mword reg_nr)
{ Cpu::wrmsr(0, _ctr_reg0+reg_nr); }

PUBLIC
void
Perf_cnt_arch::mode(Mword slot, const char **mode, 
		    Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  static const char * const mode_str[2][2][2] =
    { { { "off", "off" }, { "d.K",   "e.K"   } },
      { { "d.U", "e.U" }, { "d.K+U", "e.K+U" } } };

  *mode    = mode_str[(int)pmc_event[slot].user]
		     [(int)pmc_event[slot].kern]
		     [(int)pmc_event[slot].edge];
  *event   = pmc_event[slot].evnt;
  *user    = pmc_event[slot].user;
  *kern    = pmc_event[slot].kern;
  *edge    = pmc_event[slot].edge;
}

PUBLIC
void
Perf_cnt_arch::setup_pmc(Mword slot, Mword bitmask, 
			 Mword event, Mword user, Mword kern, Mword edge)
{
  if (alloc_pmc(slot, bitmask))
    {
      pmc_event[slot].user    = user;
      pmc_event[slot].kern    = kern;
      pmc_event[slot].edge    = edge;
      pmc_event[slot].evnt    = event;
      pmc_event[slot].bitmask = bitmask;
      set_pmc_event(slot);
      clear_pmc(pmc_event[slot].pmc);
      start_pmc(pmc_event[slot].pmc);
    }
}

PUBLIC virtual
void
Perf_cnt_arch::start_pmc(Mword /*reg_nr*/)
{
  // nothing to do per default
}

// watchdog supported by performance counter architecture?
PUBLIC inline
int
Perf_cnt_arch::have_watchdog()
{ return _watchdog; }

PUBLIC
void
Perf_cnt_arch::setup_watchdog(Mword timeout)
{
  alloc_watchdog();
  if (watchdog_allocated())
    {
      hold_watchdog = ((Signed64)((Cpu::boot_cpu()->frequency() >> 16) * timeout)) << 16;
      // The maximum value a performance counter register can be written to
      // is 0x7ffffffff. The 31st bit is extracted to the bits 32-39 (see 
      // "IA-32 Intel Architecture Software Developer's Manual. Volume 3: 
      // Programming Guide" section 14.10.2: PerfCtr0 and PerfCtr1 MSRs.
      if (hold_watchdog > 0x7fffffff)
	hold_watchdog = 0x7fffffff;
      hold_watchdog = -hold_watchdog;
      init_watchdog();
      touch_watchdog();
      start_watchdog();
      start_pmc(pmc_watchdog);
    }
}

PUBLIC
void
Perf_cnt_arch::setup_loadcnt()
{
  alloc_loadcnt();
  if (loadcnt_allocated())
    {
      init_loadcnt();
      start_pmc(pmc_loadcnt);
    }
}

PUBLIC virtual
void
Perf_cnt_arch::init_watchdog()
{} // no watchdog per default

PUBLIC virtual
void
Perf_cnt_arch::init_loadcnt()
{ panic("Cannot initialize load counter"); }

// start watchdog (enable generation of overflow interrupt)
PUBLIC virtual
void
Perf_cnt_arch::start_watchdog()
{} // no watchdog per default

// stop watchdog (disable generation of overflow interrupt)
PUBLIC virtual
void
Perf_cnt_arch::stop_watchdog()
{} // no watchdog per default

//--------------------------------------------------------------------

STATIC_INITIALIZE_P(Perf_cnt, PERF_CNT_INIT_PRIO);

// basic perfcounter detection
PUBLIC static FIASCO_INIT_CPU
void
Perf_cnt::init()
{
  Cpu const &cpu = *Cpu::boot_cpu();
  Mword perfctr_type = Perfctr_x86_generic;
  Unsigned32 eax, ebx, ecx;

  for (Mword i=0; i<Perf_cnt::Max_slot; i++)
    read_pmc_fn[i] = dummy_read_pmc;

  if (cpu.tsc() && cpu.can_wrmsr())
    {
      cpu.arch_perfmon_info(&eax, &ebx, &ecx);
      if ((eax & 0xff) && (((eax>>8) & 0xff) > 1))
        {
          perfctr_type  = Perfctr_x86_arch_perfmon;
          perf_type_str = "PA";
          read_pmc_fns  = p6_read_pmc_fns;
          pcnt          = &perf_cnt_ap;
        }
      if (perfctr_type == Perfctr_x86_generic)
        {
          if (cpu.vendor() == cpu.Vendor_intel)
            {
              // Intel
              switch (cpu.family())
                {
                case 5:
                perf_event_type  = P5;
                if (cpu.local_features() & Cpu::Lf_rdpmc)
                  {
                    perfctr_type  = Perfctr_x86_intel_p5mmx;
                    perf_type_str = "P5MMX";
                    read_pmc_fns  = p6_read_pmc_fns;
                  }
                else
                  {
                    perfctr_type  = Perfctr_x86_intel_p5;
                    perf_type_str = "P5";
                    read_pmc_fns  = p5_read_pmc_fns;
                  }
                pcnt = &perf_cnt_p5;
                break;

                case 6:
                  perf_event_type = P6;
                  if (cpu.model() == 9 || cpu.model() == 13)
                    {
                      perfctr_type  = Perfctr_x86_intel_pentm;
                      perf_type_str = "PntM";
                    }
                  else if (cpu.model() >= 7)
                    {
                      perfctr_type  = Perfctr_x86_intel_piii;
                      perf_type_str = "PIII";
                    }
                  else if (cpu.model() >= 3)
                    {
                      perfctr_type  = Perfctr_x86_intel_pii;
                      perf_type_str = "PII";
                    }
                  else
                    {
                      perfctr_type  = Perfctr_x86_intel_p6;
                      perf_type_str = "PPro";
                    }
                  read_pmc_fns = p6_read_pmc_fns;
                  pcnt = &perf_cnt_p6;
                  break;

                case 15:
                  perf_event_type = P4;
                  if (cpu.model() >= 3)
                    {
                      perfctr_type = Perfctr_x86_intel_p4m3;
                      perf_type_str = "P4M3";
                    }
                  else if (cpu.model() >= 2)
                    {
                      perfctr_type = Perfctr_x86_intel_p4m2;
                      perf_type_str = "P4M2";
                    }
                  else
                    {
                      perfctr_type = Perfctr_x86_intel_p4;
                      perf_type_str = "P4";
                    }
                  read_pmc_fns = p4_read_pmc_fns;
                  pcnt = &perf_cnt_p4;
                  break;
                }
            }
          else if (cpu.vendor() == Cpu::Vendor_amd)
            {
              // AMD
              switch (cpu.family())
                {
                case 6:
                case 15:
                case 16:
                  if (cpu.family() == 15)
                    {
                      perf_type_str = "K8";
                      perfctr_type  = Perfctr_x86_amd_k8;
                    }
                  else
                    {
                      perf_type_str = "K7";
                      perfctr_type  = Perfctr_x86_amd_k7;
                    }
                  perf_event_type = P6;
                  read_pmc_fns    = k7_read_pmc_fns;
                  pcnt            = &perf_cnt_k7;
                  break;
                }
            }
        }

      // set PCE-Flag in CR4 to enable read of performance measurement
      // counters in usermode. PMC were introduced in Pentium MMX and
      // PPro processors.
      if (cpu.local_features() & Cpu::Lf_rdpmc)
        cpu.enable_rdpmc();
    }

  if (pcnt && !pcnt->init())
    {
      perfctr_type = Perfctr_x86_generic;
      pcnt         = 0;  // init failed, no performance counters available
    }

  if (perfctr_cpu_event_set != 0 && perfctr_cpu_event_set(perfctr_type) == 0)
    {
      perfctr_type = Perfctr_x86_generic;
      pcnt         = 0;  // init failed, no performance counters available
    }

  // tell perflib the cpu type
  if (perfctr_set_cputype != 0)
    perfctr_set_cputype(perfctr_type);

}

PUBLIC static inline void FIASCO_INIT_CPU
Perf_cnt::init_ap()
{
  if (Perf_cnt::pcnt)
    {
      Perf_cnt::pcnt->init();
      Perf_cnt::pcnt->init_loadcnt();
      Perf_cnt::pcnt->start_pmc(0);
    }
}

PUBLIC static inline NOEXPORT void
Perf_cnt::set_pmc_fn(Mword slot, Mword nr)
{ read_pmc_fn[slot] = read_pmc_fns[nr]; }

// watchdog supported by performance counter architecture?
PUBLIC static inline
int
Perf_cnt::have_watchdog()
{ return (pcnt && pcnt->have_watchdog()); }

// setup watchdog function with timeout in seconds
PUBLIC static inline
void
Perf_cnt::setup_watchdog(Mword timeout)
{
  if (pcnt)
    pcnt->setup_watchdog(timeout);
}

PUBLIC static inline
void
Perf_cnt::setup_loadcnt()
{
  if (pcnt)
    pcnt->setup_loadcnt();
}

PUBLIC static inline
void
Perf_cnt::start_watchdog()
{
  if (pcnt && pcnt->watchdog_allocated())
    {
      pcnt->touch_watchdog();
      pcnt->start_watchdog();
    }
}

PUBLIC static inline
void
Perf_cnt::stop_watchdog()
{
  if (pcnt && pcnt->watchdog_allocated())
    pcnt->stop_watchdog();
}

PUBLIC static inline
void
Perf_cnt::touch_watchdog()
{
  if (pcnt && pcnt->watchdog_allocated())
    pcnt->touch_watchdog();
}

// return human-readable type of performance counters
PUBLIC static inline
char const *
Perf_cnt::perf_type()
{ return perf_type_str; }

// set performance counter counting the selected event in slot #slot
PUBLIC static
int
Perf_cnt::setup_pmc(Mword slot, Mword event, Mword user, Mword kern, Mword edge)
{
  if (!pcnt)
    return 0;

  unsigned nr, evntsel;
  Mword bitmask, unit_mask;
  const struct perfctr_event *pe = 0;

  split_event(event, &evntsel, &unit_mask);
  if (perfctr_lookup_event != 0)
    pe = perfctr_lookup_event(evntsel, &nr);
  bitmask = pe ? pe->counters_set : 0xffff;
  pcnt->setup_pmc(slot, bitmask, event, user, kern, edge);
  Tb_entry::set_rdcnt(slot, (kern | user) ? read_pmc_fn[slot] : 0);
  return 1;
}

// return current selected event for a slot #slot
PUBLIC static
int
Perf_cnt::mode(Mword slot, const char **mode, const char **name, 
	       Mword *event, Mword *user, Mword *kern, Mword *edge)
{
  if (!perf_type() || !pcnt)
    {
      *mode  = "off";
      *event = *user = *kern  = 0;
      return 0;
    }

  unsigned nr, evntsel;
  Mword unit_mask;
  const struct perfctr_event *pe = 0;

  pcnt->mode(slot, mode, event, user, kern, edge);
  split_event(*event, &evntsel, &unit_mask);
  if (perfctr_lookup_event != 0)
    pe = perfctr_lookup_event(evntsel, &nr);
  *name  = pe ? pe->name : "";
  return 1;
}

PUBLIC static Mword
Perf_cnt::get_max_perf_event()
{ return (perfctr_get_max_event != 0) ? perfctr_get_max_event() : 0; }

PUBLIC static void
Perf_cnt::get_perf_event(Mword nr, unsigned *evntsel, 
			 const char **name, const char **desc)
{
  const struct perfctr_event *pe = 0;

  if (perfctr_index_event != 0)
    pe = perfctr_index_event(nr);

  *name    = pe ? pe->name        : 0;
  *desc    = pe ? pe->description : 0;
  *evntsel = pe ? pe->evntsel     : 0;
}

PUBLIC static Mword
Perf_cnt::lookup_event(unsigned evntsel)
{
  unsigned nr;

  if (perfctr_lookup_event != 0 && perfctr_lookup_event(evntsel, &nr) != 0)
    return nr;
  return (Mword)-1;
}

PUBLIC static void
Perf_cnt::get_unit_mask(Mword nr, Unit_mask_type *type,
			Mword *default_value, Mword *nvalues)
{
  const struct perfctr_event *event = 0;
  
  if (perfctr_index_event != 0) 
    event = perfctr_index_event(nr);

  *type = None;
  if (event && event->unit_mask)
    {
      *default_value = event->unit_mask->default_value;
      switch (event->unit_mask->type)
	{
	case perfctr_um_type_fixed:	*type = Fixed; break;
	case perfctr_um_type_exclusive:	*type = Exclusive; break;
	case perfctr_um_type_bitmask:	*type = Bitmask; break;
	}
      *nvalues = event->unit_mask->nvalues;
    }
}

PUBLIC static void
Perf_cnt::get_unit_mask_entry(Mword nr, Mword idx, 
			      Mword *value, const char **desc)
{
  const struct perfctr_event *event = 0;
  
  if (perfctr_index_event != 0)
    event = perfctr_index_event(nr);

  *value = 0;
  *desc  = 0;
  if (event && event->unit_mask && (idx < event->unit_mask->nvalues))
    {
      *value = event->unit_mask->values[idx].value;
      *desc  = event->unit_mask->values[idx].description;
    }
}

/** Split event into event selector and unit mask (depending on perftype). */
PUBLIC static
void
Perf_cnt::split_event(Mword event, unsigned *evntsel, Mword *unit_mask)
{
  switch (perf_event_type)
    {
    case P5:
      *evntsel   = event;
      *unit_mask = 0;
      break;
    case P6:
      *evntsel   =  event & 0x000000ff;
      *unit_mask = (event & 0x0000ff00) >> 8;
      break;
    case P4:
    default:
      *evntsel   = 0;
      *unit_mask = 0;
      break;
    }
}

/** Combine event from selector and unit mask. */
PUBLIC static
void
Perf_cnt::combine_event(Mword evntsel, Mword unit_mask, Mword *event)
{
  switch (perf_event_type)
    {
    case P5:
      *event = evntsel;
      break;
    case P6:
      *event = (evntsel & 0x000000ff) + ((unit_mask & 0x000000ff) << 8);
      break;
    case P4:
      break;
    }
}
