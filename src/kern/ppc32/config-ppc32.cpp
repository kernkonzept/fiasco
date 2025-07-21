INTERFACE [ppc32]:

#include <cstring>

EXTENSION class Config
{
public:
  enum
  {
    // cannot access user memory directly
    Access_user_mem = No_access_user_mem,

    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,

    SUPERPAGE_SHIFT = 22,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,

    Irq_shortcut = 0, //TODO: set
  };

  enum
  {
    Kmem_size     = 8*1024*1024,
    Htab_entries  = 8, //number of entries in page-table-entry group
                       //, min=1, max=8
  };

  enum
  {
    Scheduler_granularity	= CONFIG_SCHED_GRANULARITY,
    Default_time_slice	        = CONFIG_SCHED_DEF_TIME_SLICE * Scheduler_granularity,
  };

  static unsigned const default_console_uart = 3;
  static unsigned const default_console_uart_baudrate = 115200;
};


//---------------------------------------------------------------------------
IMPLEMENTATION [ppc32]:

const char *const Config::kernel_warn_config_string = 0;

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{}
