IMPLEMENTATION [{ia32,ux}-debug]:

#include <cstring>
#include "types.h"

IMPLEMENT
void
Kip::debug_print_syscalls() const
{
  unsigned kips = kip_sys_calls;
  static char const* const KIPS[] = {"No KIP syscalls supported",
				     "KIP syscalls via KIP relative stubs",
				     "KIP syscalls via absolute stubs",
				     "KIP syscalls ERROR: bad value"};

  if (kips > 3)
    kips = 3;

  printf("%s\n", KIPS[kips]);
}
