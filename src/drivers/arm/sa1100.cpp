INTERFACE [pf_sa1100]:

#include "types.h"
#include "mmio_register_block.h"

class Sa1100 : public Mmio_register_block
{
public:
  enum {
    RSRR = 0x030000,

    /* interrupt controller */
    ICIP = 0x050000,
    ICMR = 0x050004,
    ICLR = 0x050008,
    ICCR = 0x05000c,
    ICFP = 0x050010,
    ICPR = 0x050020,

    /* OS Timer */
    OSMR0 = 0x000000,
    OSMR1 = 0x000004,
    OSMR2 = 0x000008,
    OSMR3 = 0x00000c,
    OSCR  = 0x000010,
    OSSR  = 0x000014,
    OWER  = 0x000018,
    OIER  = 0x00001c,

    RSRR_SWR = 1,
  };

  Sa1100(Address base) : Mmio_register_block(base) {}
};

