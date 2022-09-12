INTERFACE [arm && pf_s32z]:

#define TARGET_NAME "NXP S32Z2/S32E2"

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32z && pf_s32z_rtu_all]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(8, Max_num_cpus) };
};

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32z && pf_s32z_rtu_0]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 0, Max_num_nodes = min<int>(4, Max_num_cpus) };
};

// ------------------------------------------------------------------------
INTERFACE [arm && amp && pf_s32z && pf_s32z_rtu_1]:

#include "minmax.h"

EXTENSION class Config
{
public:
  enum { First_node = 4, Max_num_nodes = min<int>(4, Max_num_cpus) };
};

