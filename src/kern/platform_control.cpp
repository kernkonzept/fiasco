INTERFACE:

#include "types.h"

class Platform_control
{
public:
  static void init(Cpu_number);
  static bool cpu_shutdown_available();
  static int cpu_allow_shutdown(Cpu_number cpu, bool allow);
  static int system_suspend(Mword extra);
};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "l4_types.h"

IMPLEMENT_DEFAULT inline
void
Platform_control::init(Cpu_number)
{}

IMPLEMENT_DEFAULT inline NEEDS["l4_types.h"]
int
Platform_control::system_suspend(Mword)
{ return -L4_err::EBusy; }

IMPLEMENT_DEFAULT inline
bool
Platform_control::cpu_shutdown_available()
{ return false; }

IMPLEMENT_DEFAULT inline NEEDS["l4_types.h"]
int
Platform_control::cpu_allow_shutdown(Cpu_number, bool)
{ return -L4_err::ENodev; }

