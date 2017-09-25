INTERFACE:

#include "initcalls.h"
#include "types.h"

class Cpu;

class Utcb_init
{
public:
  /**
   * UTCB access initialization.
   *
   * Allocates the UTCB pointer page and maps it to Kmem::utcb_ptr_page.
   * Setup both segment selector and gs register to allow gs:0 access.
   *
   * @post user can access the utcb pointer via gs:0.
   */
  static void init() FIASCO_INIT;
};
