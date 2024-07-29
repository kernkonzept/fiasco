IMPLEMENTATION[arm]:

IMPLEMENT
void
Jdb_kern_info_bench::show_arch()
{}

IMPLEMENTATION[arm && pf_realview]:

#include "platform.h"

IMPLEMENT inline NEEDS["platform.h"]
Unsigned64
Jdb_kern_info_bench::get_time_now()
{ return Platform::sys->read<Mword>(Platform::Sys::Cnt_24mhz); }

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
