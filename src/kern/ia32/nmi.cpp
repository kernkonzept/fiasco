INTERFACE [ia32 || amd64]:

#include "per_cpu_data.h"
#include "types.h"

/**
 * Handling of NMIs which are not consumed by a kernel debugger.
 *
 * NMIs are recorded per CPU and printed later from the timer interrupt.
 */
class Nmi
{
public:
  enum Reason
  {
    Unknown,
    Serr,
    Iochk,
    Nr_reasons,
  };

private:
  struct Stats
  {
    Unsigned32 count[Nr_reasons] = {};
    Unsigned32 reported[Nr_reasons] = {};
    Mword last_ip = 0;
    Unsigned64 window_start = 0;
    unsigned window_count = 0;
  };

  enum
  {
    /// Maximum number of NMIs per second on one CPU before a reboot.
    Storm_limit        = 1000,

    /// NMI status and control register.
    Port_status        = 0x61,
    Status_serr        = 0x80,
    Status_iochk       = 0x40,
    Ctrl_serr_disable  = 0x04,
    Ctrl_iochk_disable = 0x08,
  };

  static Per_cpu<Stats> _stats;
  static Mword _pending;
  static Mword _reporting;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include <cstdio>
#include "atomic.h"
#include "cpu.h"
#include "io.h"
#include "l4_types.h"
#include "mem.h"
#include "platform_control.h"

DEFINE_PER_CPU Per_cpu<Nmi::Stats> Nmi::_stats;
Mword Nmi::_pending;
Mword Nmi::_reporting;

/**
 * Handle an NMI which is not consumed by a kernel debugger.
 *
 * Records the NMI for a later report. Reboots the system if this is
 * configured or if NMIs arrive at a high rate on this CPU.
 *
 * \param cpu  Current CPU.
 * \param ip   Instruction pointer of the interrupted code.
 */
PUBLIC static
void
Nmi::handle(Cpu_number cpu, Mword ip)
{
  Stats &s = _stats.cpu(cpu);
  Reason r = platform_reason();

  write_now(&s.count[r], s.count[r] + 1);
  write_now(&s.last_ip, ip);
  Mem::mp_wmb();
  write_now(&_pending, Mword{true});

  if (TAG_ENABLED(nmi_reboot) || storm(&s))
    Platform_control::system_reboot();
}

/**
 * Print the NMIs recorded since the last report.
 *
 * \pre Must not be called from NMI context.
 */
PUBLIC static inline
void
Nmi::report()
{
  if (!access_once(&_pending)) [[likely]]
    return;

  do_report();
}

PRIVATE static
void
Nmi::do_report()
{
  if (!cas(&_reporting, Mword{false}, Mword{true}))
    return;

  write_now(&_pending, Mword{false});
  Mem::mp_mb();

  static char const *const names[Nr_reasons] = { "unknown", "SERR#", "IOCHK#" };

  for (Cpu_number cpu : Cpu::online_mask())
    {
      Stats &s = _stats.cpu(cpu);
      for (unsigned r = 0; r < Nr_reasons; ++r)
        {
          Unsigned32 n = access_once(&s.count[r]);
          if (n == s.reported[r])
            continue;

          printf("NMI: CPU%u: %u NMI(s) from %s source, last IP " L4_PTR_FMT
                 "\n", cxx::int_value<Cpu_number>(cpu), n - s.reported[r],
                 names[r], access_once(&s.last_ip));
          s.reported[r] = n;
        }
    }

  Mem::mp_mb();
  write_now(&_reporting, Mword{false});
}

/**
 * Determine the platform source of the current NMI and clear it.
 */
PRIVATE static
Nmi::Reason
Nmi::platform_reason()
{
  Unsigned8 status = Io::in8(Port_status);
  if (status == 0xff)
    return Unknown;

  if (status & Status_serr)
    {
      clear_source(status, Ctrl_serr_disable);
      return Serr;
    }

  if (status & Status_iochk)
    {
      clear_source(status, Ctrl_iochk_disable);
      return Iochk;
    }

  return Unknown;
}

/**
 * Clear the latched NMI source by disabling and re-enabling it.
 */
PRIVATE static
void
Nmi::clear_source(Unsigned8 status, Unsigned8 disable)
{
  Unsigned8 ctrl = (status & 0x0f) | disable;
  Io::out8(ctrl, Port_status);
  Io::out8(ctrl & ~disable, Port_status);
}

/**
 * Count the NMI in the current one-second window.
 *
 * \retval true   More than Storm_limit NMIs in the current window.
 * \retval false  Otherwise.
 */
PRIVATE static
bool
Nmi::storm(Stats *s)
{
  Unsigned64 now = Cpu::rdtsc();
  if (now - s->window_start > Cpu::ns_to_tsc(1000000000ULL))
    {
      s->window_start = now;
      s->window_count = 0;
    }

  return ++s->window_count > Storm_limit;
}
