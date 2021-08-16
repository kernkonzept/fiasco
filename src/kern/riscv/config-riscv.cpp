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
  enum : Mword
  {
    // cannot access user memory directly
    Access_user_mem = No_access_user_mem,

    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1UL << PAGE_SHIFT,
    PAGE_MASK  = ~(PAGE_SIZE - 1),

    SUPERPAGE_SIZE  = 1UL << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK  = ~(SUPERPAGE_SIZE - 1),

    hlt_works_ok = 1,
    Irq_shortcut = 1,
  };

  enum
  {
    KMEM_SIZE = 16 << 20,
  };

  static unsigned const default_console_uart = 0;
  static unsigned const default_console_uart_baudrate = 115200;
};

//---------------------------------------------------------------------------
INTERFACE [riscv && emulate_periodic_timer]:

EXTENSION class Config
{
public:
  enum
  {
    Scheduler_one_shot    = 0,
    Scheduler_granularity = 1000UL,
    Default_time_slice    = 10 * Scheduler_granularity,
  };
};

//---------------------------------------------------------------------------
INTERFACE [riscv && !emulate_periodic_timer]:

EXTENSION class Config
{
public:
  enum
  {
    Scheduler_one_shot    = 1,
    Scheduler_granularity = 1UL,
    Default_time_slice    = 10000 * Scheduler_granularity,
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [riscv]:

const char *const Config::kernel_warn_config_string = 0;

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{}
