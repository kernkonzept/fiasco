INTERFACE[ux]:

#include "std_macros.h"

IMPLEMENTATION[ux]:

#include "context.h"
#include "globals.h"
#include <cstdlib>

FIASCO_NORETURN
void
terminate(int x)
{
  exit(x);
}
