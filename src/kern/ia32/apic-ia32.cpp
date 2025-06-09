INTERFACE:

#include <cxx/cxx_int>
#include "alternatives.h"
#include "per_cpu_data.h"
#include "types.h"
#include "initcalls.h"
#include "pm.h"

class Return_frame;
class Cpu;

struct Apic_id : cxx::int_type<Unsigned32, Apic_id>
{
  Apic_id() = default;
  Apic_id(Unsigned32 n) : cxx::int_type<Unsigned32, Apic_id>(n) {}
};

class Apic : public Pm_object
{
  friend class Jdb_kern_info_io_apic;

public:
  static void map_registers() FIASCO_INIT;
  static void detect_x2apic() FIASCO_INIT;
  static void init(bool resume = false) FIASCO_INIT_AND_PM;
  Apic_id apic_id() const { return _id; }

  static Per_cpu<Static_object<Apic> > apic;

  // A write of 0 to effectively stops the Local APIC timer.
  static constexpr Unsigned32 Timer_min = 1;
  static constexpr Unsigned32 Timer_max = UINT32_MAX;

  struct use_x2apic : public Alternative_static_functor<use_x2apic>
  {
    static bool probe() { return Apic::use_x2; }
  };

private:
  Apic(const Apic&) = delete;
  Apic &operator = (Apic const &) = delete;

  Apic_id _id;
  Unsigned32 _saved_apic_timer;

  static void error_interrupt(Return_frame *regs)
    asm ("apic_error_interrupt") FIASCO_FASTCALL;

  static unsigned present;      // CPU feature bit FEAT_APIC set
  static int good_cpu;          // CPU could be capable of having Local APIC
  static void *io_base asm ("apic_io_base");
  static Address phys_base;
  static unsigned timer_divisor;
  static unsigned frequency_khz;
  static Unsigned64 scaler_us_to_apic;
  static bool use_x2 asm ("apic_use_x2");

  enum class Reg : unsigned
  {
    Id      = 0x20,     // Local APIC ID
    Lvr     = 0x30,     // Local APIC version
    Tpr     = 0x80,     // Task priority register
    Ppr     = 0xA0,     // Processor priority register
    Eoi     = 0xB0,     // EOI register
    Ldr     = 0xD0,     // Local destination register
    Dfr     = 0xE0,     // Destination format register
    Spiv    = 0xF0,     // Spurious interrupt vector register
    Isr     = 0x100,    // In-service register
    Tmr     = 0x180,    // Trigger mode register
    Irr     = 0x200,    // Interrupt request register
    Esr     = 0x280,    // Error status register
    Icr     = 0x300,    // Interrupt command register (bits 0-31)
    Icr2    = 0x310,    // Interrupt command register (bits 32-63)
    Lvtt    = 0x320,    // LVT timer register
    Lvtthmr = 0x330,    // LVT thermal sensor register
    Lvtpc   = 0x340,    // LVT performance monitoring counters register
    Lvt0    = 0x350,    // LVT LINT0 register
    Lvt1    = 0x360,    // LVT LINT1 register
    Lvterr  = 0x370,    // LVT error register
    Tmict   = 0x380,    // Timer: Initial count register
    Tmcct   = 0x390,    // Timer: Current count register
    Tdcr    = 0x3E0,    // Timer: Divide configuration register
  };

  enum
  {
    APIC_tpri_mask		= 0xFF,
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
    APIC_msr_base               = 0x800,
  };

  enum
  {
    Present = 0x01,
    Present_before_msr = 0x02,
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

PUBLIC static
Cpu_number
Apic::find_cpu(Apic_id phys_id)
{
  return apic.find_cpu([phys_id](Apic const *a)
                       { return a && a->apic_id() == phys_id; });
}

PUBLIC void
Apic::pm_on_suspend(Cpu_number) override
{
  _saved_apic_timer = timer_reg_read();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PUBLIC void
Apic::pm_on_resume(Cpu_number) override
{
  Apic::init(true);
  timer_reg_write(_saved_apic_timer);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [mp]:

PUBLIC void
Apic::pm_on_resume(Cpu_number cpu) override
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
      : "A" (us),   "g" (static_cast<Unsigned32>(scaler_us_to_apic))
        // scaler_us_to_apic is actually 32-bit
       );
  return apic;
}

//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

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
#include "kmem_mmio.h"
#include "kip.h"
#include "lock_guard.h"
#include "panic.h"
#include "processor.h"
#include "regdefs.h"
#include "pic.h"
#include "pit.h"
#include "warn.h"

unsigned apic_spurious_interrupt_bug_cnt;
unsigned apic_spurious_interrupt_cnt;
unsigned apic_error_cnt;

unsigned Apic::present;
int Apic::good_cpu;
void *Apic::io_base;
Address Apic::phys_base;
unsigned Apic::timer_divisor = 1;
unsigned Apic::frequency_khz;
Unsigned64 Apic::scaler_us_to_apic;
bool       Apic::use_x2;

int ignore_invalid_apic_reg_access;

/**
 * CPU identifier of this CPU as provided by the APIC.
 */
PUBLIC inline
Cpu_phys_id
Apic::cpu_id() const
{
  if (use_x2apic())
    return Cpu_phys_id{cxx::int_value<Apic_id>(_id)};
  else
    return Cpu_phys_id{cxx::int_value<Apic_id>(_id) >> 24};
}

/**
 * APIC identifier of the current CPU.
 */
PUBLIC static inline
Apic_id
Apic::get_id()
{
  if (use_x2apic())
    return Apic_id{reg_read(Reg::Id)};
  else
    return Apic_id{reg_read(Reg::Id) & 0xff000000};
}

/**
 * APIC identifier generated from 1-byte MADT Local APIC structure APIC ID.
 */
PUBLIC static inline
Apic_id
Apic::acpi_lapic_to_apic_id(Unsigned32 acpi_id)
{
  if (use_x2apic())
    return Apic_id{acpi_id};
  else
    return Apic_id{Unsigned32{acpi_id} << 24};
}

PRIVATE static inline
Unsigned32
Apic::get_version()
{
  return reg_read(Reg::Lvr) & 0xFF;
}

PRIVATE static inline NOEXPORT
int
Apic::is_integrated()
{
  return reg_read(Reg::Lvr) & 0xF0;
}

PRIVATE static inline NOEXPORT
Unsigned32
Apic::get_max_lvt_local()
{
  return ((reg_read(Reg::Lvr) >> 16) & 0xFF);
}

PRIVATE static inline NOEXPORT
Unsigned32
Apic::get_num_errors()
{
  reg_write(Reg::Esr, 0);
  return reg_read(Reg::Esr);
}

PRIVATE static inline NOEXPORT
void
Apic::clear_num_errors()
{
  reg_write(Reg::Esr, 0);
  reg_write(Reg::Esr, 0);
}

PUBLIC static inline
unsigned
Apic::get_frequency_khz()
{
  return frequency_khz;
}

PUBLIC static inline
Unsigned32
Apic::reg_read(Apic::Reg reg)
{
  unsigned reg_offs = static_cast<unsigned>(reg);
  if (use_x2apic())
    return Cpu::rdmsr(Msr::X2apic_regs, reg_offs >> 4);
  else
    return *offset_cast<volatile Unsigned32 *>(io_base, reg_offs);
}

PUBLIC static inline
void
Apic::reg_write(Apic::Reg reg, Unsigned32 val)
{
  unsigned reg_offs = static_cast<unsigned>(reg);
  if (use_x2apic())
    Cpu::wrmsr(val, 0, Msr::X2apic_regs, reg_offs >> 4);
  else
    *offset_cast<volatile Unsigned32 *>(io_base, reg_offs) = val;
}

PUBLIC static inline NEEDS[<cassert>]
void
Apic::reg_write64(Apic::Reg reg, Unsigned32 low, Unsigned32 high)
{
  unsigned reg_offs = static_cast<unsigned>(reg);
  assert(use_x2apic());
  Unsigned64 val = (Unsigned64{high} << 32) | low;
  Cpu::wrmsr(val, Msr::X2apic_regs, reg_offs >> 4);
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
  return reg_read(Reg::Tmcct);
}

PUBLIC static inline
Unsigned32
Apic::timer_reg_read_initial()
{
  return reg_read(Reg::Tmict);
}

PUBLIC static inline
void
Apic::timer_reg_write(Unsigned32 val)
{
  reg_read(Reg::Tmict);
  reg_write(Reg::Tmict, val);
}

PUBLIC static inline NEEDS["cpu.h"]
Address
Apic::apic_page_phys()
{ return Cpu::rdmsr(Msr::Ia32_apic_base) & 0xfffff000; }

// set the global pagetable entry for the Local APIC device registers
PUBLIC
static FIASCO_INIT_AND_PM
void
Apic::map_apic_page()
{
  Address base = apic_page_phys();
  // We should not change the physical address of the Local APIC page if
  // possible since some versions of VMware would complain about a
  // non-implemented feature
  assert(!io_base); // Ensure only called once
  io_base = Kmem_mmio::map(base, Config::PAGE_SIZE);
  if (!io_base)
    panic("Unable to map local APIC");

  Kip::k()->add_mem_region(Mem_desc(base, base + Config::PAGE_SIZE - 1, Mem_desc::Reserved));
}

// check CPU type if APIC could be present
static FIASCO_INIT_AND_PM
int
Apic::test_cpu(Cpu *cpu)
{
  if (!cpu->can_wrmsr() || !(cpu->features() & FEAT_TSC))
    return 0;

  if (cpu->vendor() == Cpu::Vendor_intel)
    {
      if (cpu->family() == 15)
	return 1;
      if (cpu->family() >= 6)
	return 1;
    }
  if (cpu->vendor() == Cpu::Vendor_amd && cpu->family() >= 6)
    return 1;

  return 0;
}

PUBLIC static inline
void
Apic::timer_enable_irq()
{
  Unsigned32 tmp_val;

  tmp_val = reg_read(Reg::Lvtt);
  tmp_val &= ~APIC_lvt_masked;
  reg_write(Reg::Lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::timer_disable_irq()
{
  Unsigned32 tmp_val;

  tmp_val = reg_read(Reg::Lvtt);
  tmp_val |= APIC_lvt_masked;
  reg_write(Reg::Lvtt, tmp_val);
}

PUBLIC static inline
int
Apic::timer_is_irq_enabled()
{
  return ~reg_read(Reg::Lvtt) & APIC_lvt_masked;
}

PUBLIC static inline
void
Apic::timer_set_periodic()
{
  Unsigned32 tmp_val = reg_read(Reg::Lvtt);
  tmp_val |= APIC_lvt_timer_periodic;
  reg_write(Reg::Lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::timer_set_one_shot()
{
  Unsigned32 tmp_val = reg_read(Reg::Lvtt);
  tmp_val &= ~APIC_lvt_timer_periodic;
  reg_write(Reg::Lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::timer_assign_irq_vector(unsigned vector)
{
  Unsigned32 tmp_val = reg_read(Reg::Lvtt);
  tmp_val &= 0xffffff00;
  tmp_val |= vector;
  reg_write(Reg::Lvtt, tmp_val);
}

PUBLIC static inline
void
Apic::irq_ack()
{
  reg_read(Reg::Spiv);
  reg_write(Reg::Eoi, 0);
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
      tmp_value = reg_read(Reg::Tdcr);
      tmp_value &= ~0x1F;
      tmp_value |= div;
      reg_write(Reg::Tdcr, tmp_value);
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
  return (is_present() && (get_max_lvt() >= 4));
}

PUBLIC static inline
int
Apic::have_tsint()
{
  return (is_present() && (get_max_lvt() >= 5));
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
  Unsigned32 val = reg_read(Reg::Spiv);

  // Enable APIC.
  val |= (1 << 8);

  // Enable Focus Processor Checking.
  val &= ~(1 << 9);

  // Set spurious IRQ vector to 0x3f.
  // Bits 0 .. 3 are hardwired to 1 on PPro!
  val &= ~0xff;
  val |= APIC_IRQ_BASE + 0xf;

  reg_write(Reg::Spiv, val);
}

static FIASCO_INIT_AND_PM
void
Apic::done_spiv()
{
  Unsigned32 val = reg_read(Reg::Spiv);
  val &= ~(1 << 8);
  reg_write(Reg::Spiv, val);
}

PUBLIC static inline NEEDS[Apic::reg_write]
void
Apic::tpr(unsigned prio)
{ reg_write(Reg::Tpr, prio); }

PUBLIC static inline NEEDS[Apic::reg_read]
unsigned
Apic::tpr()
{ return reg_read(Reg::Tpr); }

static FIASCO_INIT_CPU_AND_PM
void
Apic::init_tpr()
{
  reg_write(Reg::Tpr, 0);
}

// activate APIC error interrupt
static FIASCO_INIT_CPU_AND_PM
void
Apic::enable_errors()
{
  if (is_integrated())
    {
      if (get_max_lvt() > 3)
        clear_num_errors();

      Unsigned32 before = get_num_errors();
      Unsigned32 val = reg_read(Reg::Lvterr);

      // Unmask error IRQ vector.
      val &= 0xfffeff00;

      // Set error IRQ vector to 0x63.
      val |= APIC_IRQ_BASE + 3;

      reg_write(Reg::Lvterr, val);

      if (get_max_lvt() > 3)
        clear_num_errors();

      Unsigned32 after = get_num_errors();

      if (Warn::is_enabled(Info))
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
  tmp_val = reg_read(Reg::Lvt0);
  tmp_val &= 0xfffe5800;
  tmp_val |= 0x00000700;
  reg_write(Reg::Lvt0, tmp_val);

  // set LINT1 to NMI, edge triggered
  tmp_val = reg_read(Reg::Lvt1);
  tmp_val &= 0xfffe5800;
  tmp_val |= 0x00000400;
  reg_write(Reg::Lvt1, tmp_val);

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
  reg_write(Reg::Lvtt, reg_read(Reg::Lvtt) | APIC_lvt_masked | 0xff);

  if (have_pcint())
    // mask performance interrupt and set vector to a valid value
    reg_write(Reg::Lvtpc, reg_read(Reg::Lvtpc) | APIC_lvt_masked | 0xff);

  if (have_tsint())
    // mask thermal sensor interrupt and set vector to a valid value
    reg_write(Reg::Lvtthmr, reg_read(Reg::Lvtthmr) | APIC_lvt_masked | 0xff);
}

// give us a hint if we have an APIC but it is disabled (debugging only)
PUBLIC static
int
Apic::test_present_but_disabled()
{
  if (!good_cpu)
    return 0;

  Unsigned64 msr = Cpu::rdmsr(Msr::Ia32_apic_base);
  return ((msr & 0xffffff000ULL) == 0xfee00000ULL);
}

// activate APIC by writing to appropriate MSR
PUBLIC static FIASCO_INIT_CPU_AND_PM
void
Apic::activate_by_msr()
{
  Unsigned64 msr;

  msr = Cpu::rdmsr(Msr::Ia32_apic_base);
  phys_base = msr & 0xfffff000;
  msr |= 1 << 11; // enable local APIC
  if (use_x2)
    msr |= 1 << 10; // enable x2APIC mode
  Cpu::wrmsr(msr, Msr::Ia32_apic_base);

  // later we have to call update_feature_info() as the flags may have changed
}

PUBLIC static
inline int
Apic::is_present()
{
  return ((present & Present) == Present);
}

PUBLIC static
void
Apic::set_perf_nmi()
{
  if (have_pcint())
    reg_write(Reg::Lvtpc, 0x400);
}

static FIASCO_INIT_CPU_AND_PM
void
Apic::calibrate_timer(Cpu *cpu)
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

      if (cpu->tsc_frequency_accurate())
        {
          auto guard = lock_guard(cpu_lock);
          tt1 = timer_reg_read();
          cpu->busy_wait_ns(50000000ULL);  // 20Hz
          tt2 = timer_reg_read();
          count = cpu->ns_to_tsc(50000000ULL);
        }
      else
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
      // ignore possible invalid access which may happen when JDB tries to
      // access non-existent memory
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

PRIVATE static FIASCO_INIT_CPU_AND_PM
void
Apic::init_timer(Cpu *cpu)
{
  calibrate_timer(cpu);
  timer_set_divisor(1);
  enable_errors();
}

PUBLIC static
void
Apic::dump_info()
{
  if (Warn::is_enabled(Info))
    printf("Local APIC[%02x]: version=%02x max_lvt=%d\n",
           cxx::int_value<Apic_id>(get_id()) >> 24, get_version(), get_max_lvt());
}

IMPLEMENT
void
Apic::detect_x2apic()
{
  if (Cpu::cpuid_ecx(1) & FEATX_X2APIC)
    use_x2 = true;
}

/**
 * Map the Local APIC device registers.
 *
 * This method needs to be called before Local APIC registers are accessed
 * (which includes the Apic::get_id() and Apic::get_version() methods) and
 * also before the Apic::init() method is called. It runs the basic detection
 * of the Local APIC presence and maps its device registers.
 */
IMPLEMENT
void
Apic::map_registers()
{
  Cpu *cpu = Cpu::boot_cpu();

  if (cpu->features() & FEAT_APIC)
    {
      present |= Present;
      present |= Present_before_msr;
    }
  else
    {
      present &= ~Present;
      present &= ~Present_before_msr;
    }

  if (use_x2)
    {
      printf("Using x2APIC\n");
      present |= Present;
      activate_by_msr();
    }
  else
    {
      printf("Using Legacy xAPIC\n");
      if (!(present & Present_before_msr))
        {
          good_cpu = test_cpu(cpu);
          if (good_cpu && Config::apic)
            {
              // activate; this could lead an disabled APIC to appear
              // set MMIO base address to be able to access the registers
              activate_by_msr();
              cpu->update_features_info();
              if (cpu->features() & FEAT_APIC)
                present |= Present;
            }
        }
    }

  if (!Config::apic)
    return;

  // initialize if available
  if (is_present() && !use_x2)
    map_apic_page();
}

/**
 * Initialize the Local APIC.
 *
 * This method does the initialization of the Local APIC either on bootstrap
 * or on power resume. Before this method is called for the first time,
 * the Apic::map_registers() method needs to be called.
 *
 * \param resume Initialization after power resume.
 */
IMPLEMENT
void
Apic::init(bool resume)
{
  Cpu *cpu = Cpu::boot_cpu();

  // FIXME: reset cached CPU features, we should add a special function for the
  //        APIC bit
  if (resume)
    cpu->update_features_info();

  if (!Config::apic)
    return;

  if (is_present())
    {
      // The firmware or the boot loader might have enabled the APIC before.
      // Deactivate it before initializing it properly to avoid conflicting
      // configurations.
      if (!resume)
        done_spiv();

      init_lvt();
      init_spiv();
      init_tpr();

      // test if local timer counts down
      if (check_working())
        {
          if (!(present & Present_before_msr))
            // APIC was not present *before* writing to MSR but is now present.
            // We have to set LVT0 and LVT1 to appropriate values.
            route_pic_through_apic();
        }
      else
        present &= ~Present;
    }

  if (!is_present())
    panic("Local APIC not found");

  dump_info();

  init_timer(cpu);
}
