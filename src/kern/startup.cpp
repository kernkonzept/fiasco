INTERFACE:

class Startup
{
public:
  // startup code for the early startup phase
  // this code is executed before anything else, inparticular before
  // any I/O is initialized
  static void stage1();

  // startup code that runs after console I/O is initialized
  static void stage2();
};

IMPLEMENTATION:

#include "static_init.h"

STATIC_INITIALIZEX_P(Startup, stage1, STARTUP1_INIT_PRIO);
STATIC_INITIALIZEX_P(Startup, stage2, STARTUP_INIT_PRIO);
