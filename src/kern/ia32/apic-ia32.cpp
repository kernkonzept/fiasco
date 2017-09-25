INTERFACE:

#include "per_cpu_data.h"
#include "types.h"
#include "initcalls.h"
#include "pm.h"

class Return_frame;
class Cpu;

class Apic : public Pm_object
{
public:
  static void init(bool resume = false) FIASCO_INIT_AND_PM;
  Unsigned32 apic_id() const { return _id; }
  Unsigned32 cpu_id() const { return _id >> 24; }

  static Per_cpu<Static_object<Apic> > apic;

private:
  Apic(const Apic&) = delete;
  Apic &operator = (Apic const &) = delete;

  Unsigned32 _id;
  Unsigned32 _saved_apic_timer;

  static void			error_interrupt(Return_frame *regs)
				asm ("apic_error_interrupt") FIASCO_FASTCALL;

  static int			present;
  static int			good_cpu;
  static const			Address io_base;
  static Address		phys_base;
  static unsigned		timer_divisor;
  static unsigned		frequency_khz;
  static Unsigned64		scaler_us_to_apic;

  enum
  {
    APIC_id			= 0x20,
    APIC_lvr			= 0x30,
    APIC_tpr                    = 0x80,
    APIC_tpri_mask		= 0xFF,
    APIC_eoi			= 0xB0,
    APIC_ldr			= 0xD0,
    APIC_ldr_mask		= 0xFFul << 24,
    APIC_dfr			= 0xE0,
    APIC_spiv			= 0xF0,
    APIC_isr			= 0x100,
    APIC_tmr			= 0x180,
    APIC_irr			= 0x200,
    APIC_esr			= 0x280,
    APIC_lvtt			= 0x320,
    APIC_lvtthmr		= 0x330,
    APIC_lvtpc			= 0x340,
    APIC_lvt0			= 0x350,
    APIC_timer_base_div		= 0x2,
    APIC_lvt1			= 0x360,
    APIC_lvterr			= 0x370,
    APIC_tmict			= 0x380,
    APIC_tmcct			= 0x390,
    APIC_tdcr			= 0x3E0,

    APIC_snd_pending		= 1 << 12,
    APIC_input_polarity		= 1 << 13,
    APIC_lvt_remote_irr		= 1 << 14,
    APIC_lvt_level_trigger	= 1 << 15,
    APIC_lvt_masked		= 1 << 16,
    APIC_lvt_timer_periodic	= 1 << 17,
    APIC_tdr_div_1		= 0xB,
    APIC_tdr_div_2		= 0x0,
    APIC_tdr_div_4		= 0x1,
    APIC_tdr_div_8		= 0x2,
    APIC_tdr_div_16		= 0x3,
    APIC_tdr_div_32		= 0x8,
    APIC_tdr_div_64		= 0x9,
    APIC_tdr_div_128		= 0xA,
  };

  enum
  {
    Mask			=  1,
    Trigger_mode		=  2,
    Remote_irr			=  4,
    Pin_polarity		=  8,
    Delivery_state		= 16,
    Delivery_mode		= 32,
  };

  enum
  {
    APIC_base_msr		= 0x1b,
  };
};

extern unsigned apic_spurious_interrupt_bug_cnt;
extern unsigned apic_spurious_interrupt_cnt;
extern unsigned apic_error_cnt;


//----------------------------------------------------------------------------
IMPLEMENTATION:
#include "cpu.h"

DEFINE_PER_CPU Per_cpu<Static_object<Apic> >  Apic::apic;

PUBLIC inline
Apic::Apic(Cpu_number cpu) : _id(get_id()) { register_pm(cpu); }


PRIVATE static
Cpu &
Apic::cpu() { return *Cpu::boot_cpu(); }

// FIXME: workaround for missing lambdas in gcc < 4.5
namespace {
struct By_id
{
  Unsigned32 p;
  By_id(Unsigned32 p) : p(p) {}
  bool operator () (Apic const *a) const { return a && a->apic_id() == p; }
};
}

PUBLIC static
Cpu_number
Apic::find_cpu(Unsigned32 phys_id)
{
  return apic.find_cpu(By_id(phys_id));
}

PUBLIC void
Apic::pm_on_suspend(Cpu_number)
{
  _saved_apic_timer = timer_reg_read();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PUBLIC void
Apic::pm_on_resume(Cpu_number)
{
  Apic::init(true);
  timer_reg_write(_saved_apic_timer);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

PUBLIC void
Apic::pm_on_resume(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    Apic::init(true);
  else
    Apic::init_ap();
  timer_reg_write(_saved_apic_timer);
}


//----------------------------------------------------------------------------
IMPLEMENTATION[ia32]:

PUBLIC static inline
Unsigned32
Apic::us_to_apic(Unsigned64 us)
{
  Unsigned32 apic, dummy1, dummy2;
  asm ("movl  %%edx, %%ecx		\n\t"
       "mull  %4			\n\t"
       "movl  %%ecx, %%eax		\n\t"
       "movl  %%edx, %%ecx		\n\t"
       "mull  %4			\n\t"
       "addl  %%ecx, %%eax		\n\t"
       "shll  $11, %%eax		\n\t"
      :"=a" (apic), "=d" (dummy1), "=&c" (dummy2)
      : "A" (us),   "g" (scaler_us_to_apic)
       );
  return apic;
}

IMPLEMENTATION[amd64]:

PUBLIC static inline
Unsigned32
Apic::us_to_apic(Unsigned64 us)
{
  Unsigned32 apic, dummy;
  asm ("mulq  %3			\n\t"
       "shrq  $21,%%rax			\n\t"
      :"=a"(apic), "=d"(dummy)
      :"a"(us), "g"(scaler_us_to_apic)
      );
  return apic;
}

IMPLEMENTATION[ia32,amd64]:

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "cpu.h"
#include "cpu_lock.h"
#include "entry_frame.h"
#include "globals.h"
#include "io.h"
#include "kmem.h"
#include "kip.h"
#include "lock_guard.h"
#include "panic.h"
#include "processor.h"
#include "regdefs.h"
#include "pic.h"
#include "pit.h"

unsigned apic_spurious_interrupt_bug_cnt;
unsigned apic_spurious_interrupt_cnt;
unsigned apic_error_cnt;
Address  apic_io_base;

int        Apic::present;
int        Apic::good_cpu;
const Address Apic::io_base = Mem_layout::Local_apic_page;
Address    Apic::phys_base;
unsigned   Apic::timer_divisor = 1;
unsigned   Apic::frequency_khz;
Unsigned64 Apic::scaler_us_to_apic;

int ignore_invalid_apic_reg_access;

PUBLIC static inline
Unsigned32
Apic::get_id()
{
  return reg_read(APIC_id) & 0xff000000;
}

PRIVATE static inline
Unsigned32
Apic::get_version()
{
  return reg_read(APIC_lvr) & 0xFF;
}

PRIVATE static inline NOEXPORT
int
Apic::is_integrated()
{
  return reg_read(APIC_lvr) & 0xF0;
}

PRIVATE static inline NOEXPORT
Unsigned32
Apic::get_max_lvt_local()
{
  return ((reg_read(APIC_lvr) >> 16) & 0xFF);
}

PRIVATE static inline NOEXPORT
Unsigned32
Apic::get_num_errors()
{
  reg_write(APIC_esr, 0);
  return reg_read(APIC_esr);
}

PRIVATE static inline NOEXPORT
void
Apic::clear_num_errors()
{
  reg_write(APIC_esr, 0);
  reg_write(APIC_esr, 0);
}

PUBLIC static inline
unsigned
Apic::get_frequency_khz()
{
  return frequency_khz;
}

PUBLIC static inline
Unsigned32
Apic::reg_read(unsigned reg)
{
  return *((volatile Unsigned32*)(io_base + reg));
}

PUBLIC static inline
void
Apic::reg_write(unsigned reg, Unsigned32 val)
{
  *((volatile Unsigned32*)(io_base + reg)) = val;
}

PUBLIC static inline
int
Apic::reg_delivery_mode(Unsigned32 val)
{
  return (val >> 8) & 7;
}

PUBLIC static inline
int
Apic::reg_lvt_vector(Unsigned32 val)
{
  return val & 0xff;
}

PUBLIC static inline
Unsigned32
Apic::timer_reg_read()
{
  return reg_read(APIC_tmcct);
}

PUBLIC static inline
Unsigned32
Apic::timer_reg_read_initial()
{
  return reg_read(APIC_tmict);
}

PUBLIC static inline
void
Apic::timer_reg_write(Unsigned32 val)
{
  reg_read(APIC_tmict);
  reg_write(APIC_tmict, val);
}

PUBLIC static inline NEEDS["cpu.h"]
Address
Apic::apic_page_phys()
{ return Cpu::rdmsr(APIC_base_msr) & 0xfffff000; }

// set the global pagetable entry for the Local APIC device registers
PUBLIC
static FIASCO_INIT_AND_PM
void
Apic::map_apic_page()
{
  Address offs;
  Address base = apic_page_phys();
  // We should not change the physical address of the Local APIC page if
  // possible since some versions of VMware would complain about a
  // non-implemented feature
  Kmem::map_phys_page(base, Mem_layout::Local_apic_page,
		      false, true, &offs);

  Kip::k()->add_mem_region(Mem_desc(base, base + Config::PAGE_SIZE - 1, Mem_desc::Reserved));

  assert(offs == 0);
}

// check CPU type if APIC could be present
static FIASCO_INIT_AND_PM
int
Apic::test_cpu()
{
  if (!cpu().can_wrmsr() || !(cpu().features() & FEAT_TSC))
    return 0;

  if (cpu().vendor() == Cpu::Vendor_intel)
    {
      if (cpu().family() == 15)
	return 1;
      if (cpu().family() >= 6)
	return 1;
    }
  if (cpu().vendor() == Cpu::Vendor_amd && cpu().family() >= 6)
    return 1;

  return 0;
}

// test if APIC present
static inline
int
Apic::test_present()
{
  return cpu().features() & FEAT_APIC;
}

PUBLIC static inline
void
Apic::timer_enable_irq()
{
  Unsigned32 tmp_val;

  tmp_val = reg_read(APIC_lvtt);
  tmp_val &= ~(APIC_lvt_masked);
  reg_write(APIC_lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::timer_disable_irq()
{
  Unsigned32 tmp_val;

  tmp_val = reg_read(APIC_lvtt);
  tmp_val |= APIC_lvt_masked;
  reg_write(APIC_lvtt, tmp_val);
}

PUBLIC static inline
int
Apic::timer_is_irq_enabled()
{
  return ~reg_read(APIC_lvtt) & APIC_lvt_masked;
}

PUBLIC static inline
void
Apic::timer_set_periodic()
{
  Unsigned32 tmp_val = reg_read(APIC_lvtt);
  tmp_val |= APIC_lvt_timer_periodic;
  reg_write(APIC_lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::timer_set_one_shot()
{
  Unsigned32 tmp_val = reg_read(APIC_lvtt);
  tmp_val &= ~APIC_lvt_timer_periodic;
  reg_write(APIC_lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::timer_assign_irq_vector(unsigned vector)
{
  Unsigned32 tmp_val = reg_read(APIC_lvtt);
  tmp_val &= 0xffffff00;
  tmp_val |= vector;
  reg_write(APIC_lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::irq_ack()
{
  reg_read(APIC_spiv);
  reg_write(APIC_eoi, 0);
}

static
void
Apic::timer_set_divisor(unsigned newdiv)
{
  int i;
  int div = -1;
  int divval = newdiv;
  Unsigned32 tmp_value;

  static int divisor_tab[8] =
    {
      APIC_tdr_div_1,  APIC_tdr_div_2,  APIC_tdr_div_4,  APIC_tdr_div_8,
      APIC_tdr_div_16, APIC_tdr_div_32, APIC_tdr_div_64, APIC_tdr_div_128
    };

  for (i=0; i<8; i++)
    {
      if (divval & 1)
	{
	  if (divval & ~1)
	    {
	      printf("bad APIC divisor %u\n", newdiv);
	      return;
	    }
	  div = divisor_tab[i];
	  break;
	}
      divval >>= 1;
    }

  if (div != -1)
    {
      timer_divisor = newdiv;
      tmp_value = reg_read(APIC_tdcr);
      tmp_value &= ~0x1F;
      tmp_value |= div;
      reg_write(APIC_tdcr, tmp_value);
    }
}

static
int
Apic::get_max_lvt()
{
  return is_integrated() ? get_max_lvt_local() : 2;
}

PUBLIC static inline
int
Apic::have_pcint()
{
  return (present && (get_max_lvt() >= 4));
}

PUBLIC static inline
int
Apic::have_tsint()
{
  return (present && (get_max_lvt() >= 5));
}

// check if APIC is working (check timer functionality)
static FIASCO_INIT_AND_PM
int
Apic::check_working()
{
  Unsigned64 tsc_until;

  timer_disable_irq();
  timer_set_divisor(1);
  timer_reg_write(0x10000000);

  tsc_until = Cpu::rdtsc() + 0x400;  // we only have to wait for one bus cycle

  do
    {
      if (timer_reg_read() != 0x10000000)
        return 1;
    } while (Cpu::rdtsc() < tsc_until);

  return 0;
}

static FIASCO_INIT_CPU_AND_PM
void
Apic::init_spiv()
{
  Unsigned32 tmp_val;

  tmp_val = reg_read(APIC_spiv);
  tmp_val |= (1<<8);            // enable APIC
  tmp_val &= ~(1<<9);           // enable Focus Processor Checking
  tmp_val &= ~0xff;
  tmp_val |= APIC_IRQ_BASE + 0xf; // Set spurious IRQ vector to 0x3f
                              // bit 0..3 are hardwired to 1 on PPro!
  reg_write(APIC_spiv, tmp_val);
}

PUBLIC static inline NEEDS[Apic::reg_write]
void
Apic::tpr(unsigned prio)
{ reg_write(APIC_tpr, prio); }

PUBLIC static inline NEEDS[Apic::reg_read]
unsigned
Apic::tpr()
{ return reg_read(APIC_tpr); }

static FIASCO_INIT_CPU_AND_PM
void
Apic::init_tpr()
{
  reg_write(APIC_tpr, 0);
}

// activate APIC error interrupt
static FIASCO_INIT_CPU_AND_PM
void
Apic::enable_errors()
{
  if (is_integrated())
    {
      Unsigned32 tmp_val, before, after;

      if (get_max_lvt() > 3)
	clear_num_errors();
      before = get_num_errors();

      tmp_val = reg_read(APIC_lvterr);
      tmp_val &= 0xfffeff00;         // unmask error IRQ vector
      tmp_val |= APIC_IRQ_BASE + 3;  // Set error IRQ vector to 0x63
      reg_write(APIC_lvterr, tmp_val);

      if (get_max_lvt() > 3)
	clear_num_errors();
      after = get_num_errors();
      printf("APIC ESR value before/after enabling: %08x/%08x\n",
	    before, after);
    }
}

// activate APIC after activating by MSR was successful
// see "Intel Architecture Software Developer's Manual,
//      Volume 3: System Programming Guide, Appendix E"
static FIASCO_INIT_AND_PM
void
Apic::route_pic_through_apic()
{
  Unsigned32 tmp_val;
  auto guard = lock_guard(cpu_lock);

  // mask 8259 interrupts
  Unsigned16 old_irqs = Pic::disable_all_save();

  // set LINT0 to ExtINT, edge triggered
  tmp_val = reg_read(APIC_lvt0);
  tmp_val &= 0xfffe5800;
  tmp_val |= 0x00000700;
  reg_write(APIC_lvt0, tmp_val);

  // set LINT1 to NMI, edge triggered
  tmp_val = reg_read(APIC_lvt1);
  tmp_val &= 0xfffe5800;
  tmp_val |= 0x00000400;
  reg_write(APIC_lvt1, tmp_val);

  // unmask 8259 interrupts
  Pic::restore_all(old_irqs);

  printf("APIC was disabled --- routing PIC through APIC\n");
}

static FIASCO_INIT_CPU_AND_PM
void
Apic::init_lvt()
{
  auto guard = lock_guard(cpu_lock);

  // mask timer interrupt and set vector to _not_ invalid value
  reg_write(APIC_lvtt, reg_read(APIC_lvtt) | APIC_lvt_masked | 0xff);

  if (have_pcint())
    // mask performance interrupt and set vector to a valid value
    reg_write(APIC_lvtpc, reg_read(APIC_lvtpc) | APIC_lvt_masked | 0xff);

  if (have_tsint())
    // mask thermal sensor interrupt and set vector to a valid value
    reg_write(APIC_lvtthmr, reg_read(APIC_lvtthmr) | APIC_lvt_masked | 0xff);
}

// give us a hint if we have an APIC but it is disabled
PUBLIC static
int
Apic::test_present_but_disabled()
{
  if (!good_cpu)
    return 0;

  Unsigned64 msr = Cpu::rdmsr(APIC_base_msr);
  return ((msr & 0xffffff000ULL) == 0xfee00000ULL);
}

// activate APIC by writing to appropriate MSR
static FIASCO_INIT_CPU_AND_PM
void
Apic::activate_by_msr()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(APIC_base_msr);
  phys_base = msr & 0xfffff000;
  msr |= (1<<11);
  Cpu::wrmsr(msr, APIC_base_msr);

  // now the CPU feature flags may have changed
  cpu().update_features_info();
}

// check if we still receive interrupts after we changed the IRQ routing
PUBLIC static FIASCO_INIT_CPU
int
Apic::check_still_getting_interrupts()
{
  if (!Config::apic)
    return 0;

  Unsigned64 tsc_until;
  Cpu_time clock_start = Kip::k()->clock;

  tsc_until = Cpu::rdtsc();
  tsc_until += 0x01000000; // > 10 Mio cycles should be sufficient until
                           // we have processors with more than 10 GHz
  do
    {
      // kernel clock by timer interrupt updated?
      if (Kip::k()->clock != clock_start)
	// yes, succesful
	return 1;
    } while (Cpu::rdtsc() < tsc_until);

  // timeout
  return 0;
}

PUBLIC static
inline int
Apic::is_present()
{
  return present;
}

PUBLIC static
void
Apic::set_perf_nmi()
{
  if (have_pcint())
    reg_write(APIC_lvtpc, 0x400);
}

static FIASCO_INIT_CPU_AND_PM
void
Apic::calibrate_timer()
{
  const unsigned calibrate_time = 50;
  Unsigned32 count, tt1, tt2, result, dummy;
  Unsigned32 runs = 0, frequency_ok;

  do
    {
      frequency_khz = 0;

      timer_disable_irq();
      timer_set_divisor(1);
      timer_reg_write(1000000000);

        {
          auto guard = lock_guard(cpu_lock);

          Pit::setup_channel2_to_20hz();

          count = 0;

          tt1 = timer_reg_read();
          do
            {
              count++;
            }
          while ((Io::in8(0x61) & 0x20) == 0);
          tt2 = timer_reg_read();
        }

      result = (tt1 - tt2) * timer_divisor;

      // APIC not running
      if (count <= 1)
        return;

      asm ("divl %2"
          :"=a" (frequency_khz), "=d" (dummy)
          : "r" (calibrate_time), "a" (result), "d" (0));

      frequency_ok = (frequency_khz < (1000<<11));
    }
  while (++runs < 10 && !frequency_ok);

  if (!frequency_ok)
    panic("APIC frequency too high, adapt Apic::scaler_us_to_apic");

  Kip::k()->frequency_bus = frequency_khz;
  scaler_us_to_apic       = Cpu::muldiv(1<<21, frequency_khz, 1000);
}

IMPLEMENT
void
Apic::error_interrupt(Return_frame *regs)
{
  Unsigned32 err1, err2;

  // we are entering with disabled interrupts
  err1 = Apic::get_num_errors();
  Apic::clear_num_errors();
  err2 = Apic::get_num_errors();
  Apic::irq_ack();

  cpu_lock.clear();

  if (err1 == 0x80 || err2 == 0x80)
    {
      // ignore possible invalid access which may happen in
      // jdb::do_dump_memory()
      if (ignore_invalid_apic_reg_access)
	return;

      printf("CPU%u: APIC invalid register access error at " L4_PTR_FMT "\n",
	     cxx::int_value<Cpu_number>(current_cpu()), regs->ip());
      return;
    }

  apic_error_cnt++;
  printf("CPU%u: APIC error %08x(%08x)\n",
         cxx::int_value<Cpu_number>(current_cpu()), err1, err2);
}

// deactivate APIC by writing to appropriate MSR
PUBLIC static
void
Apic::done()
{
  Unsigned64 val;

  if (!present)
    return;

  val = reg_read(APIC_spiv);
  val &= ~(1<<8);
  reg_write(APIC_spiv, val);

  val = Cpu::rdmsr(APIC_base_msr);
  val  &= ~(1<<11);
  Cpu::wrmsr(val, APIC_base_msr);
}

PRIVATE static FIASCO_INIT_CPU_AND_PM
void
Apic::init_timer()
{
  calibrate_timer();
  timer_set_divisor(1);
  enable_errors();
}

PUBLIC static
void
Apic::dump_info()
{
  printf("Local APIC[%02x]: version=%02x max_lvt=%d\n",
         get_id() >> 24, get_version(), get_max_lvt());
}

IMPLEMENT
void
Apic::init(bool resume)
{
  int was_present;
  // FIXME: reset cached CPU features, we should add a special function
  // for the apic bit
  if(resume)
    cpu().update_features_info();

  was_present = present = test_present();

  if (!was_present)
    {
      good_cpu = test_cpu();

      if (good_cpu && Config::apic)
        {
          // activate; this could lead an disabled APIC to appear
          // set base address of I/O registers to be able to access the registers
          activate_by_msr();
          present = test_present();
        }
    }

  if (!Config::apic)
    return;

  // initialize if available
  if (present)
    {
      // map the Local APIC device registers
      if (!resume)
        map_apic_page();

      // set some interrupt vectors to appropriate values
      init_lvt();

      // initialize APIC_spiv register
      init_spiv();

      // initialize task-priority register
      init_tpr();

      // test if local timer counts down
      if ((present = check_working()))
        {
          if (!was_present)
            // APIC _was_ not present before writing to msr so we have
            // to set APIC_lvt0 and APIC_lvt1 to appropriate values
            route_pic_through_apic();
        }
    }

  if (!present)
    panic("Local APIC not found");

  dump_info();

  apic_io_base = Mem_layout::Local_apic_page;
  init_timer();
}
