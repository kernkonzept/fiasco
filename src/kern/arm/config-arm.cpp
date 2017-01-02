/* ARM specific */
INTERFACE [arm && arm_v5]:

EXTENSION class Config
{ public: enum { Access_user_mem = Must_access_user_mem_direct }; };

/* ARM specific */
INTERFACE [arm && arm_v6plus]:

EXTENSION class Config
{ public: enum { Access_user_mem = No_access_user_mem }; };

INTERFACE [arm]:

EXTENSION class Config
{
public:

  enum
  {
    PAGE_SHIFT = ARCH_PAGE_SHIFT,
    PAGE_SIZE  = 1 << PAGE_SHIFT,
    PAGE_MASK  = ~(PAGE_SIZE - 1),

    hlt_works_ok = 1,
    Irq_shortcut = 1,
  };

  enum
  {
#ifdef CONFIG_ONE_SHOT
    Scheduler_one_shot		= 1,
    Scheduler_granularity	= 1UL,
    Default_time_slice	        = 10000 * scheduler_granularity,
#else
    Scheduler_one_shot		= 0,
    Scheduler_granularity	= 1000UL,
    Default_time_slice	        = 10 * Scheduler_granularity,
#endif
  };

  enum
  {
    KMEM_SIZE = 16 << 20,
  };

  // the default uart to use for serial console
  static unsigned const default_console_uart	= 3;
  static unsigned const default_console_uart_baudrate = 115200;

  enum
  {
    Cache_enabled = true,
  };
  static const char char_micro;


  enum
  {
#ifdef CONFIG_ARM_ENABLE_SWP
    Cp15_c1_use_swp_enable = 1,
#else
    Cp15_c1_use_swp_enable = 0,
#endif
#ifdef CONFIG_ARM_ALIGNMENT_CHECK
    Cp15_c1_use_alignment_check = 1,
#else
    Cp15_c1_use_alignment_check = 0,
#endif

    Support_arm_linux_cache_API = 1,
  };

};

// -----------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

EXTENSION class Config
{
public:

  enum
  {
    SUPERPAGE_SHIFT = 21,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK  = ~(SUPERPAGE_SIZE -1)
  };
};

// -----------------------------------------------------------------------
INTERFACE [arm && !arm_lpae]:

EXTENSION class Config
{
public:

  enum
  {
    SUPERPAGE_SHIFT = 20,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
    SUPERPAGE_MASK  = ~(SUPERPAGE_SIZE -1)
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

char const Config::char_micro = '\265';
const char *const Config::kernel_warn_config_string = 0;

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm_v6plus]:

#include "feature.h"

KIP_KERNEL_FEATURE("armv6plus");
