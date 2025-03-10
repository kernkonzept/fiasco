INTERFACE [riscv && 32bit]:

EXTENSION class Config
{
public:
  enum
  {
    SUPERPAGE_SHIFT = 22,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv && 64bit && (riscv_sv39 || riscv_sv48)]:

EXTENSION class Config
{
public:
  enum
  {
    SUPERPAGE_SHIFT = 21,
  };
};

//----------------------------------------------------------------------------
INTERFACE [riscv]:

EXTENSION class Config
{
public:
  enum
  {
    // cannot access user memory directly
    Access_user_mem = No_access_user_mem,

    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1UL << PAGE_SHIFT,

    SUPERPAGE_SIZE  = 1UL << SUPERPAGE_SHIFT,

    Irq_shortcut = 1,
  };

  static unsigned const default_console_uart = 0;
  static unsigned const default_console_uart_baudrate = 115200;
};

//---------------------------------------------------------------------------
INTERFACE [riscv && !one_shot]:

EXTENSION class Config
{
public:
  enum
  {
    Scheduler_granularity = CONFIG_SCHED_GRANULARITY,
    Default_time_slice    = CONFIG_SCHED_DEF_TIME_SLICE * Scheduler_granularity,
  };
};

//---------------------------------------------------------------------------
INTERFACE [riscv && one_shot]:

EXTENSION class Config
{
public:
  enum
  {
    Scheduler_granularity = 1UL,
    Default_time_slice    = CONFIG_SCHED_DEF_TIME_SLICE * CONFIG_SCHED_GRANULARITY,
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

const char *const Config::kernel_warn_config_string = nullptr;

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{}
