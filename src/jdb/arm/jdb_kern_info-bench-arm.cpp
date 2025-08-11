IMPLEMENTATION[arm && 32bit]:

IMPLEMENT
void
Jdb_kern_info_bench::show_arch()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && 64bit]:

IMPLEMENT
void
Jdb_kern_info_bench::show_arch()
{
  static constexpr unsigned Loops = 1'000'000'000U;
#define inst_nop                                                        \
  asm volatile ("nop")

#define inst_mrs_id_aa64pfr0_el1                                        \
  asm volatile ("mrs %0, ID_AA64PFR0_EL1": "=r" (dummy))

#define BENCH(name, instruction, rounds)                                \
  do                                                                    \
    {                                                                   \
      enum { Divider = 100'000 };                                       \
      static_assert((rounds % Divider) == 0);                           \
      Unsigned64 time = get_time_now();                                 \
      for (unsigned i = rounds; i; --i)                                 \
        instruction;                                                    \
      time = get_time_now() - time;                                     \
      Unsigned64 ns_100 = time / (rounds / Divider);                    \
      printf("  %-24s  %6llu.%02llu ns\n",                              \
             name, ns_100 / 100, ns_100 % 100);                         \
    } while (0)

  Mword dummy;

  BENCH("NOP",                 inst_nop,                 Loops);
  BENCH("MRS ID_AA64PFR0_EL1", inst_mrs_id_aa64pfr0_el1, Loops);
}

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && pf_realview]:

#include "platform.h"

IMPLEMENT inline NEEDS["platform.h"]
Unsigned64
Jdb_kern_info_bench::get_time_now()
{ return Platform::sys->read<Mword>(Platform::Sys::Cnt_24mhz); }

// ------------------------------------------------------------------------
IMPLEMENTATION[arm && !pf_realview]:

#include "timer.h"

IMPLEMENT inline NEEDS["timer.h"]
Unsigned64
Jdb_kern_info_bench::get_time_now()
{
  // With CONFIG_SYNC_CLOCK=n, the system clock just reads the KIP clock which
  // is stopped while executing JDB.
  // With CONFIG_SYNC_CLOCK=y, the system clock reads the ARM generic timer.
  return Timer::system_clock();
}
