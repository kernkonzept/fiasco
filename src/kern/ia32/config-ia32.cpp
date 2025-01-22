INTERFACE[ia32 || amd64]:

#include "idt_init.h"

#define TARGET_NAME "x86"

EXTENSION class Config
{
public:

  enum
  {
    Access_user_mem = TAG_ENABLED(kernel_isolation) ? No_access_user_mem
                                                    : Access_user_mem_direct,
    Pcid_enabled = TAG_ENABLED(ia32_pcid),

    /// Timer vector used with APIC timer or IOAPIC
    Apic_timer_vector = APIC_IRQ_BASE + 0,
  };

  static unsigned scheduler_irq_vector;

  enum Scheduler_config
  {
    SCHED_PIT = 0,
    SCHED_RTC,
    SCHED_APIC,
    SCHED_HPET,

#if defined(CONFIG_SCHED_PIT)
    Scheduler_mode        = SCHED_PIT,
    Scheduler_granularity = 1000U,
    Default_time_slice    = 10 * Scheduler_granularity,
#elif defined(CONFIG_SCHED_RTC)
    Scheduler_mode = SCHED_RTC,
    Scheduler_granularity = TAG_ENABLED(slow_rtc) ? 15625U : 976U,
    Default_time_slice = 10 * Scheduler_granularity,
#elif defined(CONFIG_SCHED_APIC)
    Scheduler_mode = SCHED_APIC,
# ifdef CONFIG_ONE_SHOT
    Scheduler_granularity = 1U,
    Default_time_slice = CONFIG_SCHED_DEF_TIME_SLICE * CONFIG_SCHED_GRANULARITY,
# else
    Scheduler_granularity = CONFIG_SCHED_GRANULARITY,
    Default_time_slice = CONFIG_SCHED_DEF_TIME_SLICE * Scheduler_granularity,
# endif
#elif defined(CONFIG_SCHED_HPET)
    Scheduler_mode = SCHED_HPET,
    Scheduler_granularity = 1000U,
    Default_time_slice = 10 * Scheduler_granularity,
#endif
  };

  enum
  {
    Pic_prio_modify = true,
    Kip_clock_uses_rdtsc = TAG_ENABLED(sync_clock),
    Tsc_unified = TAG_ENABLED(tsc_unified),
  };

  static bool apic;

#ifdef CONFIG_WATCHDOG
  static bool watchdog;
#else
  static const bool watchdog = false;
#endif

  static bool hlt_works_ok;

  // the default uart to use for serial console
  static const unsigned default_console_uart = 1;
  static const unsigned default_console_uart_baudrate = 115200;

  static bool found_vmware;

  enum : unsigned int
  {
    Io_port_count = (1UL << 16),
    Io_bitmap_size = Io_port_count / 8,
  };

  using Io_bitmap = Unsigned8[Io_bitmap_size];
};

//----------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include <cstring>

bool Config::hlt_works_ok = true;

bool Config::found_vmware = false;
bool Config::apic = false;
unsigned Config::scheduler_irq_vector;

#ifdef CONFIG_WATCHDOG
bool Config::watchdog = false;
#endif

const char *const Config::kernel_warn_config_string =
#ifdef CONFIG_SCHED_RTC
  "  CONFIG_SCHED_RTC is on\n"
#endif
#ifndef CONFIG_INLINE
  "  CONFIG_INLINE is off\n"
#endif
#ifndef CONFIG_NDEBUG
  "  CONFIG_NDEBUG is off\n"
#endif
#ifndef CONFIG_NO_FRAME_PTR
  "  CONFIG_NO_FRAME_PTR is off\n"
#endif
#ifdef CONFIG_BEFORE_IRET_SANITY
  "  CONFIG_BEFORE_IRET_SANITY is on\n"
#endif
#ifdef CONFIG_FINE_GRAINED_CPUTIME
  "  CONFIG_FINE_GRAINED_CPUTIME is on\n"
#endif
#ifdef CONFIG_JDB_ACCOUNTING
  "  CONFIG_JDB_ACCOUNTING is on\n"
#endif
  "";

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{
#ifdef CONFIG_WATCHDOG
  if (Koptions::o()->opt(Koptions::F_watchdog))
    {
      watchdog = true;
      apic = true;
    }
#endif

  if (Koptions::o()->opt(Koptions::F_nohlt))
    hlt_works_ok = false;

  if (Koptions::o()->opt(Koptions::F_apic))
    apic = true;

  if (Scheduler_mode == SCHED_APIC)
    apic = true;
}

