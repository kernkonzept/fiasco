// --------------------------------------------------------------------------
IMPLEMENTATION[arm && pf_imx_6 && mptimer]:

#include "config.h"
#include "kmem.h"
#include "mem_layout.h"
#include "mmio_register_block.h"

PRIVATE static Mword Timer::interval()
{
  enum
  {
    GPT_CR  = 0x00,
    GPT_PR  = 0x04,
    GPT_SR  = 0x08,
    GPT_IR  = 0x0c,
    GPT_CNT = 0x24,

    GPT_CR_EN                 = 1 << 0,
    GPT_CR_CLKSRC_MASK        = 7 << 6,
    GPT_CR_CLKSRC_CRYSTAL_OSC = 7 << 6,
    GPT_CR_CLKSRC_32KHZ       = 4 << 6,
    GPT_CR_FRR                = 1 << 9,
    GPT_CR_RESET              = 1 << 15,

    Timer_freq = 32768,
    Ticks = 50,
    Gpt_ticks = (Timer_freq * Ticks) / Config::Scheduler_granularity,
  };

  Mmio_register_block t(Kmem::mmio_remap(Mem_layout::Gpt_phys_base));

  t.write<Mword>(0, GPT_CR);
  t.write<Mword>(GPT_CR_RESET, GPT_CR);
  while (t.read<Mword>(GPT_CR) & GPT_CR_RESET)
    ;

  t.write<Mword>(GPT_CR_CLKSRC_32KHZ | GPT_CR_FRR, GPT_CR);
  t.write<Mword>(0, GPT_PR);

  t.modify<Mword>(GPT_CR_EN, 0, GPT_CR);
  Mword vc = start_as_counter();
  while (t.read<Mword>(GPT_CNT) < Gpt_ticks)
    ;
  Mword interval = (vc - stop_counter()) / Ticks;
  t.write<Mword>(0, GPT_CR);
  return interval;
}
