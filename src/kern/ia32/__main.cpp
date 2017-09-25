INTERFACE[ia32,amd64]:
#include "types.h"
#include "initcalls.h"

IMPLEMENTATION[ia32,amd64]:
#include "boot_info.h"
#include "initcalls.h"

#include <cstdlib>
#include <cstdio>
#include <construction.h>

void kernel_main(void);

extern "C" FIASCO_FASTCALL FIASCO_INIT
void
__main(unsigned checksum_ro)
{
  /* set global to be used in the constructors */
  Boot_info::set_checksum_ro(checksum_ro);
  Boot_info::init();

  atexit(&static_destruction);
  static_construction();

  kernel_main();
  exit(0);
}
