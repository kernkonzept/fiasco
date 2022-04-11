INTERFACE [arm]:

#include "irq_chip.h"

EXTENSION class Timer
{
  friend class Kip_test;

public:
  static Irq_chip::Mode irq_mode();
  static Unsigned32 get_scaler_ts_to_ns() { return _scaler_ts_to_ns; }
  static Unsigned32 get_shift_ts_to_ns() { return _shift_ts_to_ns; }
  static Unsigned32 get_scaler_ts_to_us() { return _scaler_ts_to_us; }
  static Unsigned32 get_shift_ts_to_us() { return _shift_ts_to_us; }

private:
  static inline void update_one_shot(Unsigned64 wakeup);
  static Unsigned64 time_stamp();
  static Unsigned32 _scaler_ts_to_ns;
  static Unsigned32 _scaler_ts_to_us;
  static Unsigned32 _shift_ts_to_ns;
  static Unsigned32 _shift_ts_to_us;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && vcache]:

#include "mem_unit.h"

PRIVATE static inline NEEDS["mem_unit.h"]
void
Timer::kipclock_cache()
{
  Mem_unit::clean_dcache((void *)&Kip::k()->_clock);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !vcache]:

PRIVATE static inline
void
Timer::kipclock_cache()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "config.h"
#include "context_base.h"
#include "globals.h"
#include "kip.h"
#include "watchdog.h"
#include "warn.h"

Unsigned32 Timer::_scaler_ts_to_ns;
Unsigned32 Timer::_scaler_ts_to_us;
Unsigned32 Timer::_shift_ts_to_ns;
Unsigned32 Timer::_shift_ts_to_us;

IMPLEMENT_DEFAULT
Unsigned64
Timer::time_stamp()
{ return 0; }

IMPLEMENT_DEFAULT
Irq_chip::Mode Timer::irq_mode()
{ return Irq_chip::Mode::F_raising_edge; }

IMPLEMENT inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock()
{
  if (Config::Kip_clock_uses_timer)
    {
      Cpu_time time = ts_to_us(time_stamp());
      Kip::k()->set_clock(time);
      if (time >= Kip::Clock_1_year)
        WARN("KIP clock initialized to %llu on boot CPU\n", time);
    }
  else
    Kip::k()->set_clock(0);
}

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock_ap(Cpu_number cpu)
{
  if (Config::Kip_clock_uses_timer)
    {
      Cpu_time time = ts_to_us(time_stamp());
      if (time >= Kip::Clock_1_year)
        WARN("KIP clock initialized to %llu on CPU%u\n",
             time, cxx::int_value<Cpu_number>(cpu));
    }
}

IMPLEMENT inline NEEDS["config.h", "globals.h", "kip.h", "watchdog.h", Timer::kipclock_cache]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      if (Config::Kip_clock_uses_timer)
        Kip::k()->set_clock(ts_to_us(time_stamp()));
      else
        Kip::k()->add_to_clock(Config::Scheduler_granularity);
      kipclock_cache();
      Watchdog::touch();
    }
}

IMPLEMENT_DEFAULT inline
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{}

IMPLEMENT_DEFAULT inline NEEDS["config.h", "context_base.h", "kip.h"]
Unsigned64
Timer::system_clock()
{
  if (Config::Scheduler_one_shot)
    return 0;
  if (current_cpu() == Cpu_number::boot_cpu()
      && Config::Kip_clock_uses_timer)
    {
      Cpu_time time = ts_to_us(time_stamp());
      Kip::k()->set_clock(time);
      kipclock_cache();
      return time;
    }

  return Kip::k()->clock();
}

IMPLEMENT inline NEEDS[Timer::update_one_shot, "config.h"]
void
Timer::update_timer(Unsigned64 wakeup)
{
  if (Config::Scheduler_one_shot)
    update_one_shot(wakeup);
}

PUBLIC static inline NEEDS[Timer::timer_value_to_time]
Unsigned64
Timer::ts_to_ns(Unsigned64 ts)
{ return timer_value_to_time(ts, _scaler_ts_to_ns, _shift_ts_to_ns); }

PUBLIC static inline NEEDS[Timer::timer_value_to_time]
Unsigned64
Timer::ts_to_us(Unsigned64 ts)
{ return timer_value_to_time(ts, _scaler_ts_to_us, _shift_ts_to_us); }

/**
 * Determine scaling factor and shift value for transforming a time stamp
 * (timer value) into a time value (microseconds or nanoseconds).
 *
 * \param period  Time period: 10^6: microseconds; 10^9: nanoseconds.
 * \param freq    Timer frequency.
 * \param scaler  Determined scaling factor (32-bit).
 * \param shift   Determined shift value (0-31).
 *
 * The following formula is used to translate a timer value into a time value:
 *
 * \code
 *             timer value * scaler                 timer value * scaler
 *   time  =  ---------------------- * 2^shift  =  ---------------------
 *                     2^32                             2^(32-shift)
 * \endcode
 *
 * The shift value is important for low timer frequencies to keep a sane amount
 * of usable digits.
 */
PRIVATE static
void
Timer::freq_to_scaler_shift(Unsigned64 period, Unsigned32 freq,
                            Unsigned32 *scaler, Unsigned32 *shift)
{
  Mword s = 0;
  while ((period / (1 << s)) / freq > 0)
    ++s;
  *scaler = (((1ULL << 32) / (1ULL << s)) * period) / freq;
  *shift = s;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

PRIVATE static inline
Unsigned64
Timer::timer_value_to_time(Unsigned64 v, Mword scaler, Mword shift)
{
  Mword lo = v & 0xffffffff;
  Mword hi = v >> 32;
  Mword dummy1, dummy2, dummy3, dummy4;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umull   %0, %3, %[scaler], %0   \n\t"
       "umull   %4, %5, %[scaler], %1   \n\t"
       "adds    %1, %4, %3              \n\t"
       "adc     %2, %5, #0              \n\t"
       "mov     %3, #32                 \n\t"
       "sub     %3, %3, %[shift]        \n\t"
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %1, LSL %[shift]\n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %2, LSL %[shift]\n\t"
       : "+r"(lo), "+r"(hi),
         "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3), "=&r"(dummy4)
       : [scaler]"r"(scaler), [shift]"r"(shift)
       : "cc");
  return ((Unsigned64)hi << 32) | lo;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

PRIVATE static inline
Unsigned64
Timer::timer_value_to_time(Unsigned64 v, Mword scaler, Mword shift)
{
  Mword dummy1, dummy2, dummy3;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umulh   %1, %0, %[scaler]       \n\t"
       "mul     %0, %0, %[scaler]       \n\t"
       "mov     %2, #32                 \n\t"
       "sub     %3, %2, %[shift]        \n\t"
       "add     %2, %2, %[shift]        \n\t"
       "lsr     %0, %0, %3              \n\t"
       "lsl     %1, %1, %2              \n\t"
       "orr     %0, %0, %1              \n\t"
       : "+r"(v), "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3)
       : [scaler]"r"(scaler), [shift]"r"(shift)
       : "cc");
  return v;
}
