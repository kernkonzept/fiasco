INTERFACE [arm]: //----------------------------------------

#include "mmio_register_block.h"

class Timer_omap_gentimer : private Mmio_register_block
{
public:
  enum
  {
    TIDR          = 0x00, // ID
    TIOCP_CFG     = 0x10, // config
    EOI           = 0x20,
    IRQSTATUS     = 0x28,
    IRQENABLE_SET = 0x2c,
    IRQWAKEEN     = 0x34,
    TCLR          = 0x38,
    TCRR          = 0x3c,
    TLDR          = 0x40,
  };
};

IMPLEMENTATION [arm]: // ----------------------------------

#include "kmem.h"
#include "mem_layout.h"

PUBLIC
Timer_omap_gentimer::Timer_omap_gentimer()
: Mmio_register_block(Kmem::mmio_remap(Mem_layout::Timergen_phys_base))
{
  // Mword idr = Io::read<Mword>(TIDR);
  // older timer: idr >> 16 == 0
  // newer timer: idr >> 16 != 0

  // reset
  write<Mword>(1, TIOCP_CFG);
  while (read<Mword>(TIOCP_CFG) & 1)
    ;
  // reset done

  // overflow mode
  write<Mword>(2, IRQENABLE_SET);
  // no wakeup
  write<Mword>(0, IRQWAKEEN);

  // program 1000 Hz timer frequency
  // (FFFFFFFFh - TLDR + 1) * timer-clock-period * clock-divider(ps)
  Mword val = 0xffffffda;
  write<Mword>(val, TLDR);
  write<Mword>(val, TCRR);

  write<Mword>(1 | 2, TCLR);
}

PUBLIC inline
void
Timer_omap_gentimer::acknowledge()
{
  write<Mword>(2, IRQSTATUS);
  write<Mword>(0, EOI);
}
