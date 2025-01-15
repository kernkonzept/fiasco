INTERFACE [mips]:

EXTENSION class Config
{
public:
  enum
  {
    //Access_user_mem = Access_user_mem_direct,
    Access_user_mem = No_access_user_mem,
    //Access_user_mem = Must_access_user_mem_direct,

    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,

    SUPERPAGE_SHIFT = 21, // MUST be a odd number!
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,

    // XXXKYMA: No large pages/TLBs yet, update paging-mips32 when enabled
    have_superpages = 0,
    Irq_shortcut = 1,
  };

  enum
  {
    Scheduler_granularity	= CONFIG_SCHED_GRANULARITY,
    Default_time_slice	        = CONFIG_SCHED_DEF_TIME_SLICE * Scheduler_granularity,
    Cpu_frequency               = CONFIG_MIPS_CPU_FREQUENCY,
  };

  enum
  {
    // Defaults for Kernel_uart ...
    default_console_uart = 0,
    default_console_uart_baudrate = 115200
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [mips]:

const char *const Config::kernel_warn_config_string = 0;

IMPLEMENT_OVERRIDE inline ALWAYS_INLINE
constexpr unsigned long Config::kmem_max() { return 32UL << 20; }

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{
  // set a smaller default for JDB trace buffers
  Config::tbuf_entries = 1024;
}
