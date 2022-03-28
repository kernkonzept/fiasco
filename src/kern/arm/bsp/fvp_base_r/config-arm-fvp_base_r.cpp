INTERFACE [arm && pf_fvp_base_r]:

#define TARGET_NAME "ARM FVP Base-R platform"

// ------------------------------------------------------------------------
INTERFACE [arm && pf_fvp_base_r && amp]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(Max_num_cpus, 4) };
};
