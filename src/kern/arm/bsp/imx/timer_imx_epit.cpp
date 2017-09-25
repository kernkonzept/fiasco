// --------------------------------------------------------------------------
INTERFACE [arm && imx_epit]:

#include "kmem.h"
#include "mem_layout.h"
#include "mmio_register_block.h"

class Timer_imx_epit : private Mmio_register_block
{
private:
  enum {
    EPITCR   = 0x00,
    EPITSR   = 0x04,
    EPITLR   = 0x08,
    EPITCMPR = 0x0c,
    EPITCNR  = 0x10,

    EPITCR_ENABLE                  = 1 << 0, // enable EPIT
    EPITCR_ENMOD                   = 1 << 1, // enable mode
    EPITCR_OCIEN                   = 1 << 2, // output compare irq enable
    EPITCR_RLD                     = 1 << 3, // reload
    EPITCR_SWR                     = 1 << 16, // software reset
    EPITCR_WAITEN                  = 1 << 19, // wait enabled
    EPITCR_CLKSRC_IPG_CLK          = 1 << 24,
    EPITCR_CLKSRC_IPG_CLK_HIGHFREQ = 2 << 24,
    EPITCR_CLKSRC_IPG_CLK_32K      = 3 << 24,
    EPITCR_PRESCALER_SHIFT         = 4,
    EPITCR_PRESCALER_MASK          = ((1 << 12) - 1) << EPITCR_PRESCALER_SHIFT,

    EPITSR_OCIF = 1,
  };
};


// ------------------------------------------------------------------------
IMPLEMENTATION [arm && imx_epit]:

#include "io.h"
#include "kip.h"

PUBLIC
Timer_imx_epit::Timer_imx_epit(Address phys_base)
: Mmio_register_block(Kmem::mmio_remap(phys_base))
{
  write<Mword>(0, EPITCR); // Disable
  write<Mword>(EPITCR_SWR, EPITCR);
  while (read<Mword>(EPITCR) & EPITCR_SWR)
    ;

  write<Mword>(EPITSR_OCIF, EPITSR);

  write<Mword>(EPITCR_CLKSRC_IPG_CLK_32K
               | (0 << EPITCR_PRESCALER_SHIFT)
               | EPITCR_WAITEN
               | EPITCR_RLD
               | EPITCR_OCIEN
               | EPITCR_ENMOD,
               EPITCR);

  write<Mword>(0, EPITCMPR);

  write<Mword>(32, EPITLR);

  modify<Mword>(EPITCR_ENABLE, 0, EPITCR);
}

PUBLIC inline
void
Timer_imx_epit::acknowledge()
{
  write<Mword>(EPITSR_OCIF, EPITSR);
}

