// ------------------------------------------------------------------------
INTERFACE [arm && pf_mps3_an536]:

EXTENSION class Platform_control
{
  enum { Max_cores = 2 };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pf_mps3_an536 && amp]:

#include "static_init.h"

PUBLIC static void
Platform_control::amp_boot_init()
{ amp_boot_ap_cpus(Max_cores); }

static void
setup_amp()
{ Platform_control::amp_prepare_ap_cpus(); }

STATIC_INITIALIZER_P(setup_amp, EARLY_INIT_PRIO);
