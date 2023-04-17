INTERFACE [arm]:

#include "per_cpu_data.h"

extern Per_cpu<Mword> num_serrors;

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

DEFINE_PER_CPU Per_cpu<Mword> num_serrors;
