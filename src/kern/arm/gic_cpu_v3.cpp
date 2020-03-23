INTERFACE:

#include "types.h"

class Gic_cpu_v3
{
public:
  enum
  {
    Cpu_prio_val      = 0xf0,
  };

  enum
  {
    ICC_SRE_SRE          = 1 << 0,
    ICC_SRE_DFB          = 1 << 1,
    ICC_SRE_DIB          = 1 << 2,
    ICC_SRE_Enable_lower = 1 << 3,
  };

  void disable() {}
};

