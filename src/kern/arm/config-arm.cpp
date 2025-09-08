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

    Irq_shortcut = 1,
  };

  enum
  {
#ifdef CONFIG_ONE_SHOT
    Scheduler_granularity = 1UL,
    Default_time_slice = CONFIG_SCHED_DEF_TIME_SLICE * CONFIG_SCHED_GRANULARITY,
#else
    Scheduler_granularity = CONFIG_SCHED_GRANULARITY,
    Default_time_slice = CONFIG_SCHED_DEF_TIME_SLICE * Scheduler_granularity,
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
    Sctlr_use_swp_enable = TAG_ENABLED(arm_enable_swp),
    Sctlr_use_alignment_check = TAG_ENABLED(arm_alignment_check),
    Kip_clock_uses_timer = TAG_ENABLED(sync_clock),
    Fast_interrupts = TAG_ENABLED(arm_fast_interrupts),
  };
};

// Macro to force 32-bit instructions, even on Thumb builds.
#ifdef __thumb__
#define INST32(inst)  inst ".w"
#else
#define INST32(inst)  inst
#endif

// -----------------------------------------------------------------------
INTERFACE [arm && mmu && arm_lpae]:

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
INTERFACE [arm && mmu && !arm_lpae]:

EXTENSION class Config
{
public:

  enum
  {
    SUPERPAGE_SHIFT = 20,
    SUPERPAGE_SIZE  = 1 << SUPERPAGE_SHIFT,
  };
};

// -----------------------------------------------------------------------
INTERFACE [arm && !mmu]:

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
INTERFACE [arm && 32bit]:

EXTENSION class Config
{
public:
  // Attention: this enum is used by the Lauterbach Trace32 OS awareness.
  enum Ext_vcpu_info
  {
    Ext_vcpu_infos_offset = 0x100,
    Ext_vcpu_state_offset = 0x180,
  };
};

// -----------------------------------------------------------------------
INTERFACE [arm && 64bit]:

EXTENSION class Config
{
public:
  // Attention: this enum is used by the Lauterbach Trace32 OS awareness.
  enum Ext_vcpu_info
  {
    Ext_vcpu_infos_offset = 0x200,
    Ext_vcpu_state_offset = 0x280,
  };
};

// -----------------------------------------------------------------------
INTERFACE [arm && cpu_virt && arm_cortex_r52]:

EXTENSION class Config
{
public:
  enum
  {
    Icache_flash_ways = CONFIG_ARM_ICACHE_FLASH_WAYS,
    Dcache_flash_ways = CONFIG_ARM_DCACHE_FLASH_WAYS,
  };
};

//---------------------------------------------------------------------------
IMPLEMENTATION [arm]:

IMPLEMENT FIASCO_INIT
void
Config::init_arch()
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm_v6plus]:

#include "feature.h"

KIP_KERNEL_FEATURE("armv6plus");

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && 32bit]:

IMPLEMENT_OVERRIDE
constexpr unsigned long Config::ext_vcpu_size() { return 0x400; }

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

IMPLEMENT_OVERRIDE
constexpr unsigned long Config::ext_vcpu_size() { return 0x800; }
