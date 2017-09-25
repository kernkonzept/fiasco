INTERFACE [abs_syscalls]:

#include "initcalls.h"

class Sys_call_page
{
public:

  static void init() FIASCO_INIT;
};

IMPLEMENTATION [abs_syscalls]:

#include "static_init.h"

#include <feature.h>
KIP_KERNEL_FEATURE("kip_syscalls");

STATIC_INITIALIZE(Sys_call_page);
