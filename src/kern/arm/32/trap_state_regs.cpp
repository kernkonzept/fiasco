INTERFACE:

#include "processor.h"

class Trap_state_regs
{
public:
  Mword pf_address;
  union
  {
    Mword error_code;
    Arm_esr esr;
  };

  Mword r[13];
};
