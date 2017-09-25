INTERFACE [watchdog && (ia32 || amd64)]:

#include "types.h"
#include "initcalls.h"

class Watchdog
{
public:
  static void (*touch)(void);
  static void (*enable)(void);
  static void (*disable)(void);
  
private:
  Watchdog();                        // default constructors are undefined
  Watchdog(const Watchdog&);

  typedef struct
    {
      unsigned active:1;
      unsigned user_active:1;
      unsigned no_user_control:1;
    } Watchdog_flags;

  static Watchdog_flags flags;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [watchdog && (ia32 || amd64)]:

#include <cstdio>
#include <cstdlib>

#include "apic.h"
#include "config.h"
#include "cpu.h"
#include "panic.h"
#include "perf_cnt.h"
#include "static_init.h"

#define WATCHDOG_TIMEOUT_S	2

void (*Watchdog::touch)(void)     = Watchdog::do_nothing;
void (*Watchdog::enable)(void)    = Watchdog::do_nothing;
void (*Watchdog::disable)(void)   = Watchdog::do_nothing;

Watchdog::Watchdog_flags Watchdog::flags;

static void
Watchdog::do_nothing()
{}

static void
Watchdog::perf_enable()
{
  if (flags.active && flags.user_active)
    {
      Perf_cnt::touch_watchdog();
      Perf_cnt::start_watchdog();
      Apic::set_perf_nmi();
    }
}

static void
Watchdog::perf_disable()
{
  if (flags.active)
    Perf_cnt::stop_watchdog();
}

static void
Watchdog::perf_touch()
{
  if (flags.active && flags.user_active && flags.no_user_control)
    Perf_cnt::touch_watchdog();
}

// user enables Watchdog
PUBLIC static inline NEEDS["perf_cnt.h"]
void
Watchdog::user_enable()
{
  flags.user_active = 1;
  Perf_cnt::start_watchdog();
}

// user disables Watchdog
PUBLIC static inline NEEDS["perf_cnt.h"]
void
Watchdog::user_disable()
{
  flags.user_active = 0;
  Perf_cnt::stop_watchdog();
}

// user takes over control of Watchdog
PUBLIC static inline
void
Watchdog::user_takeover_control()
{ flags.no_user_control = 0; }

// user gives back control of Watchdog
PUBLIC static inline
void
Watchdog::user_giveback_control()
{ flags.no_user_control = 0; }

STATIC_INITIALIZE_P(Watchdog, WATCHDOG_INIT);

PUBLIC static
void FIASCO_INIT
Watchdog::init()
{
  if (!Config::watchdog)
    return;

  printf("Watchdog: LAPIC=%s, PCINT=%s, PC=%s\n",
         Apic::is_present() ? "yes" : "no",
         Apic::have_pcint() ? "yes" : "no",
         Perf_cnt::have_watchdog() ? "yes" : "no");

  if (   !Apic::is_present()
      || !Apic::have_pcint()
      || !Perf_cnt::have_watchdog())
    panic("Cannot initialize watchdog (no/bad Local APIC)");

  // attach performance counter interrupt tom NMI
  Apic::set_perf_nmi();

  // start counter
  Perf_cnt::setup_watchdog(WATCHDOG_TIMEOUT_S);

  touch   = perf_touch;
  enable  = perf_enable;
  disable = perf_disable;

  flags.active = 1;
  flags.user_active = 1;
  flags.no_user_control = 1;

  printf("Watchdog initialized\n");
}
