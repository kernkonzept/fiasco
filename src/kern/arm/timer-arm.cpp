INTERFACE [arm]:

#include "irq_chip.h"
#include "global_data.h"

EXTENSION class Timer
{
  friend class Kip_test;
  struct Scaler_shift
  {
    Unsigned32 scaler;
    Unsigned32 shift;
  };

public:
  static Irq_chip::Mode irq_mode();
  static Scaler_shift get_scaler_shift_ts_to_ns() { return _scaler_shift_ts_to_ns; }
  static Scaler_shift get_scaler_shift_ts_to_us() { return _scaler_shift_ts_to_us; }

private:
  static inline void update_one_shot(Unsigned64 wakeup);
  static Unsigned64 time_stamp();
  static Global_data<Scaler_shift> _scaler_shift_ts_to_ns;
  static Global_data<Scaler_shift> _scaler_shift_ts_to_us;
  static Global_data<Scaler_shift> _scaler_shift_us_to_ts;
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && vcache]:

#include "mem_unit.h"

PRIVATE static inline NEEDS["mem_unit.h"]
void
Timer::kipclock_cache()
{
  Mem_unit::clean_dcache(static_cast<void *>(const_cast<Cpu_time *>(&Kip::k()->_clock)));
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
#include "kip.h"
#include "watchdog.h"
#include "warn.h"

DEFINE_GLOBAL Global_data<Timer::Scaler_shift> Timer::_scaler_shift_ts_to_ns;
DEFINE_GLOBAL Global_data<Timer::Scaler_shift> Timer::_scaler_shift_ts_to_us;
DEFINE_GLOBAL Global_data<Timer::Scaler_shift> Timer::_scaler_shift_us_to_ts;

IMPLEMENT_DEFAULT
Unsigned64
Timer::time_stamp()
{ return 0; }

IMPLEMENT_DEFAULT
Irq_chip::Mode Timer::irq_mode()
{ return Irq_chip::Mode::F_raising_edge; }

IMPLEMENT_DEFAULT inline
void
Timer::update_one_shot(Unsigned64 /*wakeup*/)
{}

IMPLEMENT inline NEEDS[Timer::update_one_shot, "config.h"]
void
Timer::update_timer(Unsigned64 wakeup)
{
  if (Config::Scheduler_one_shot)
    update_one_shot(wakeup);
}

PUBLIC static inline NEEDS[Timer::apply_scaler_shift]
Unsigned64
Timer::ts_to_ns(Unsigned64 ts)
{ return apply_scaler_shift(ts, _scaler_shift_ts_to_ns); }

PUBLIC static inline NEEDS[Timer::apply_scaler_shift]
Unsigned64
Timer::ts_to_us(Unsigned64 ts)
{ return apply_scaler_shift(ts, _scaler_shift_ts_to_us); }

PUBLIC static inline NEEDS[Timer::apply_scaler_shift]
Unsigned64
Timer::us_to_ts(Unsigned64 ts)
{ return apply_scaler_shift(ts, _scaler_shift_us_to_ts); }

/**
 * Determine scaling factor and shift value used for transforming a time stamp
 * (timer value) into a time value (microseconds or nanoseconds) or a time value
 * into a time stamp.
 *
 * \param from_freq     Source frequency.
 * \param to_freq       Target frequency.
 * \param scaler_shift  Determined scaling factor and shift value.
 *
 * To determine scaler/shift for converting an arbitrary timer frequency into
 * microseconds, use `to_freq=1'000'000` and `from_freq=<timer frequency>`. To
 * determine scaler/shift for converting microseconds into timer ticks, use
 * `to_freq=<timer frequency>` and `from_freq=1'000'000>`.
 *
 * The following formula is used to translate a time value `tv_from` into a time
 * value `tv_to` using `scaler` and `shift`:
 *
 * \code
 *              tv_from * scaler                 tv_from * scaler
 *   tv_to  =  ------------------ * 2^shift  =  ------------------
 *                    2^32                         2^(32-shift)
 * \endcode
 *
 * The shift value is important for low timer frequencies to keep a sane amount
 * of usable digits.
 */
PRIVATE static
void
Timer::calc_scaler_shift(Unsigned32 from_freq, Unsigned32 to_freq,
                         Scaler_shift *scaler_shift)
{
  Mword s = 0;
  while ((to_freq / (1 << s)) / from_freq > 0)
    ++s;
  scaler_shift->scaler = (((1ULL << 32) / (1ULL << s)) * to_freq) / from_freq;
  scaler_shift->shift = s;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !sync_clock]:

IMPLEMENT inline NEEDS["config.h", "kip.h", "watchdog.h", Timer::kipclock_cache]
void
Timer::update_system_clock(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    {
      Kip::k()->add_to_clock(Config::Scheduler_granularity);
      kipclock_cache();
      Watchdog::touch();
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && sync_clock]:

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock()
{
  Cpu_time time = ts_to_us(time_stamp());
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on boot CPU\n", time);
}

IMPLEMENT_OVERRIDE inline NEEDS["kip.h", "warn.h"]
void
Timer::init_system_clock_ap(Cpu_number cpu)
{
  Cpu_time time = ts_to_us(time_stamp());
  if (time >= Kip::Clock_1_year)
    WARN("System clock initialized to %llu on CPU%u\n",
         time, cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE inline
Unsigned64
Timer::system_clock()
{
  return ts_to_us(time_stamp());
}

IMPLEMENT inline
Unsigned64
Timer::aux_clock_unstopped()
{
  return ts_to_us(time_stamp());
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

PRIVATE static inline
Unsigned64
Timer::apply_scaler_shift(Unsigned64 v, Scaler_shift scaler_shift)
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
#ifdef __thumb__
       "lsl     %4, %1, %[shift]        \n\t"
       "lsl     %5, %2, %[shift]        \n\t"
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %4              \n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %5              \n\t"
#else
       "lsr     %0, %0, %3              \n\t"
       "orr     %0, %0, %1, LSL %[shift]\n\t"
       "lsr     %1, %1, %3              \n\t"
       "orr     %1, %1, %2, LSL %[shift]\n\t"
#endif
       : "+r"(lo), "+r"(hi),
         "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3), "=&r"(dummy4)
       : [scaler]"r"(scaler_shift.scaler), [shift]"r"(scaler_shift.shift)
       : "cc");
  return (Unsigned64{hi} << 32) | lo;
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

PRIVATE static inline
Unsigned64
Timer::apply_scaler_shift(Unsigned64 v, Scaler_shift scaler_shift)
{
  Mword dummy1, dummy2, dummy3;
  // This code is written in Assembler so that it doesn't require libgcc. It
  // should be also very similar to kip_time_fn_read_us()/kip_time_fn_read_us()
  // used by userland to get the same results as the kernel code.
  asm ("umulh   %1, %0, %x[scaler]      \n\t"
       "mul     %0, %0, %x[scaler]      \n\t"
       "mov     %2, #32                 \n\t"
       "sub     %3, %2, %x[shift]       \n\t"
       "add     %2, %2, %x[shift]       \n\t"
       "lsr     %0, %0, %3              \n\t"
       "lsl     %1, %1, %2              \n\t"
       "orr     %0, %0, %1              \n\t"
       : "+r"(v), "=&r"(dummy1), "=&r"(dummy2), "=&r"(dummy3)
       : [scaler]"r"(scaler_shift.scaler), [shift]"r"(scaler_shift.shift)
       : "cc");
  return v;
}
