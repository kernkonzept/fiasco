/* IA32/AMD64 specific */
INTERFACE[ia32,amd64]:

#include "idt_init.h"

EXTENSION class Config
{
public:

  enum
  {
    // can access user memory directly
    Access_user_mem = Access_user_mem_direct,

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

#ifdef CONFIG_SCHED_PIT
    Scheduler_mode        = SCHED_PIT,
    Scheduler_granularity = 1000U,
    Default_time_slice    = 10 * Scheduler_granularity,
#endif

#ifdef CONFIG_ONE_SHOT
    Scheduler_one_shot = true,
#else
    Scheduler_one_shot = false,
#endif

#ifdef CONFIG_SCHED_RTC
    Scheduler_mode = SCHED_RTC,
#  ifdef CONFIG_SLOW_RTC
    Scheduler_granularity = 15625U,
#  else
    Scheduler_granularity = 976U,
#  endif
    Default_time_slice = 10 * Scheduler_granularity,
#endif

#ifdef CONFIG_SCHED_APIC
    Scheduler_mode = SCHED_APIC,
#  ifdef CONFIG_ONE_SHOT
    Scheduler_granularity = 1U,
    Default_time_slice = 10000 * Scheduler_granularity,
#  else
    Scheduler_granularity = 1000U,
    Default_time_slice = 10 * Scheduler_granularity,
#  endif
#endif

#ifdef CONFIG_SCHED_HPET
    Scheduler_mode = SCHED_HPET,
    Scheduler_granularity = 1000U,
    Default_time_slice = 10 * Scheduler_granularity,
#endif
  };

  enum
  {
    Pic_prio_modify = true,
#ifdef CONFIG_SYNC_TSC
    Kip_timer_uses_rdtsc = true,
#else
    Kip_timer_uses_rdtsc = false,
#endif
  };

  static bool apic;

#ifdef CONFIG_WATCHDOG
  static bool watchdog;
#else
  static const bool watchdog = false;
#endif

//  static const bool hlt_works_ok = false;
  static bool hlt_works_ok;

  // the default uart to use for serial console
  static const unsigned default_console_uart = 1;
  static const unsigned default_console_uart_baudrate = 115200;

  static char const char_micro;

  static bool found_vmware;

  enum {
    Is_ux = 0,
  };
};

IMPLEMENTATION[ia32,amd64]:

#include <cstring>

bool Config::hlt_works_ok = true;

bool Config::found_vmware = false;
char const Config::char_micro = '\265';
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
#ifdef CONFIG_LIST_ALLOC_SANITY
  "  CONFIG_LIST_ALLOC_SANITY is on\n"
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

