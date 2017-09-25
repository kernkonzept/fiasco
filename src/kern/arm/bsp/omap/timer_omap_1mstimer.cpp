INTERFACE [omap3]: // ------------------------------------------------

#include "mmio_register_block.h"

class Timer_omap_1mstimer : private Mmio_register_block
{
private:
  enum {
    TIDR      = 0x000, // IP revision code
    TIOCP_CFG = 0x010, // config
    TISTAT    = 0x014, // non-interrupt status
    TISR      = 0x018, // pending interrupts
    TIER      = 0x01c, // enable/disable of interrupt events
    TWER      = 0x020, // wake-up features
    TCLR      = 0x024, // optional features
    TCRR      = 0x028, // internal counter
    TLDR      = 0x02c, // timer load value
    TTGR      = 0x030, // trigger reload by writing
    TWPS      = 0x034, // write-posted pending
    TMAR      = 0x038, // compare value
    TCAR1     = 0x03c, // first capture value of the counter
    TCAR2     = 0x044, // second capture value of the counter
    TPIR      = 0x048, // positive inc, gpt1, 2 and 10 only
    TNIR      = 0x04C, // negative inc, gpt1, 2 and 10 only
  };
};


IMPLEMENTATION [omap3]: // ------------------------------------------------

#include <cassert>
#include <cstdio>

#include "config.h"
#include "kmem.h"
#include "mem_layout.h"

PRIVATE static
void
Timer_omap_1mstimer::get_timer_values_32khz(unsigned &reload, int &tpir, int &tnir)
{
  tpir   = 232000;
  tnir   = -768000;
  reload = 0xffffffe0;
  assert(Config::Scheduler_granularity == 1000); // need to adapt here
}

PUBLIC explicit
Timer_omap_1mstimer::Timer_omap_1mstimer(bool f_32khz)
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timer1ms_phys_base))
{
  // reset
  write<Mword>(1, TIOCP_CFG);
  while (!read<Mword>(TISTAT))
    ;
  // reset done

  // overflow mode
  write<Mword>(0x2, TIER);
  // no wakeup
  write<Mword>(0x0, TWER);

  // program timer frequency
  unsigned val;
  int tpir, tnir;
  get_timer_values(val, tpir, tnir, f_32khz);

  write<Mword>(tpir, TPIR); // gpt1, gpt2 and gpt10 only
  write<Mword>(tnir, TNIR); // gpt1, gpt2 and gpt10 only
  write<Mword>(val,  TCRR);
  write<Mword>(val,  TLDR);

  // auto-reload + enable
  write<Mword>(1 | 2, TCLR);
}

PUBLIC inline
void
Timer_omap_1mstimer::acknowledge()
{
  write<Mword>(2, TISR);
}


IMPLEMENTATION [arm && omap3_am33xx]: // ----------------------------------

PRIVATE static
void
Timer_omap_1mstimer::get_timer_values(unsigned &reload, int &tpir, int &tnir,
                                      bool f_32khz)
{
  if (f_32khz)
    get_timer_values_32khz(reload, tpir, tnir);
  else
    {
      tpir   = 100000;
      tnir   = 0;
      reload = ~0 - 24 * Config::Scheduler_granularity + 1; // 24 MHz
    }
}

IMPLEMENTATION [arm && omap3_35x]: // -------------------------------------

PRIVATE static
void
Timer_omap_1mstimer::get_timer_values(unsigned &reload, int &tpir, int &tnir, bool)
{
  get_timer_values_32khz(reload, tpir, tnir);
}



