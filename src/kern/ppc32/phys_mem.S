/* Section of reserved physical-page frames */

#include "tcboffset.h"

.section .physmem,  "aw", @progbits
/* 0x0000 - 0x3000 is system reserved */
.space(VAL__KERNEL__START-0x3000)
