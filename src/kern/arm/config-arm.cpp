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

  // the default uart to use for serial console
  static unsigned const default_console_uart	= 3;
  static unsigned const default_console_uart_baudrate = 115200;

  enum
  {
    Cache_enabled = true,
  };

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
  };

  enum
  {
#ifdef CONFIG_ARM_SYNC_CLOCK
    Kip_clock_uses_timer = 1,
#else
    Kip_clock_uses_timer = 0,
#endif
  };

};

// Macro to force 32-bit instructions, even on Thumb builds.
#ifdef __thumb__
#define INST32(inst)  inst ".w"
#else
#define INST32(inst)  inst
#endif

// -----------------------------------------------------------------------
INTERFACE [arm && arm_lpae]:

EXTENSION class Config
{
public:

  enum
  {
    SUPERPAGE_SHIFT = 21,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
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
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

const char *const Config::kernel_warn_config_string = 0;

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm_v6plus]:

#include "feature.h"

KIP_KERNEL_FEATURE("armv6plus");

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 64bit]:

IMPLEMENT_OVERRIDE inline ALWAYS_INLINE
constexpr unsigned long Config::kmem_max()
{
  // The Pmem area can only map 1 GiB. Otherwise we would have to allocate
  // additional page tables, which is not implemented (yet). Also account
  // for the kernel image that is mapped in Pmem too.
  return (1024UL - 8UL) << 20;
}
