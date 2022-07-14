INTERFACE [arm && pf_sr6p7g7]:

#define TARGET_NAME "ST SR6P7G7"

// ------------------------------------------------------------------------
INTERFACE [arm && pf_sr6p7g7 && amp]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(6, Max_num_cpus) };
};
