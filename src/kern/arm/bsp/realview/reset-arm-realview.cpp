IMPLEMENTATION [arm && pf_realview]:

#include "io.h"
#include "kmem.h"
#include "platform.h"

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_realview_eb]:

static inline void do_reset()
{
  Platform::sys->write<Mword>(0x108, Platform::Sys::Reset); // the 0x100 is for Qemu
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_realview_pb11mp]:

static inline void do_reset()
{
  Platform::sys->write<Mword>(0x4, Platform::Sys::Reset);  // PORESET (0x8 would also be ok)
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_realview_pbx || pf_realview_vexpress)]:

static inline void do_reset()
{
  Platform::sys->write<Mword>(0x104, Platform::Sys::Reset); // POWER reset, 0x100 for Qemu
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_realview]:

void __attribute__ ((noreturn))
platform_reset(void)
{
  Platform::sys->write<Mword>(0xa05f, Platform::Sys::Lock);  // unlock for reset
  do_reset();

  for (;;)
    ;
}
