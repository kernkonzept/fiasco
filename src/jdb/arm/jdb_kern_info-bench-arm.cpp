IMPLEMENTATION[arm]:

IMPLEMENT
void
Jdb_kern_info_bench::show_arch()
{}

IMPLEMENTATION[arm && realview]:

#include "platform.h"

IMPLEMENT inline NEEDS["platform.h"]
Unsigned64
Jdb_kern_info_bench::get_time_now()
{ return Platform::sys->read<Mword>(Platform::Sys::Cnt_24mhz); }

IMPLEMENTATION[arm && !realview]:

#include "kip.h"

IMPLEMENT inline NEEDS["kip.h"]
Unsigned64
Jdb_kern_info_bench::get_time_now()
{ return Kip::k()->clock; }
