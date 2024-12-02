/*
 * Global kernel configuration
 */

INTERFACE:

#include <globalconfig.h>
#include "config_tcbsize.h"
#include "initcalls.h"
#include "l4_types.h"
#include "global_data.h"

// special magic to allow old compilers to inline constants

#if defined(__clang__)
# define COMPILER "clang " __clang_version__
# define GCC_VERSION 409
#else
#if defined(__GNUC__)
# define COMPILER "gcc " __VERSION__
# define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
# define COMPILER "Non-GCC"
# define GCC_VERSION 0
#endif
#endif

#define GREETING_COLOR_ANSI_OFF    "\033[0m"

#define FIASCO_KERNEL_SUBVERSION 3

class Config
{
public:

  static const char *const kernel_warn_config_string;
  enum User_memory_access_type
  {
    No_access_user_mem = 0,
    Access_user_mem_direct,
    Must_access_user_mem_direct
  };

  enum {
    SERIAL_ESC_IRQ	= 2,
    SERIAL_ESC_NOIRQ	= 1,
    SERIAL_NO_ESC	= 0,
  };

  static void init();
  static void init_arch();

  // global kernel configuration
  enum
  {
    Kernel_version_id = 0x87004444 | (FIASCO_KERNEL_SUBVERSION << 16), // "DD....."
    Kernel_prio = 0,
    Default_prio = 1,

    Warn_level = CONFIG_WARN_LEVEL,

    One_shot_min_interval_us = 200,
#ifdef CONFIG_SCHED_GRANULARITY
    Rcu_grace_period = CONFIG_SCHED_GRANULARITY,
#else
    Rcu_grace_period = 1000,
#endif

    Fine_grained_cputime = TAG_ENABLED(fine_grained_cputime),
    Stack_depth = TAG_ENABLED(stack_depth),
    Have_frame_ptr = !TAG_ENABLED(no_frame_ptr),
    Jdb = TAG_ENABLED(jdb),
    Jdb_logging = TAG_ENABLED(jdb_logging),
    Jdb_accounting = TAG_ENABLED(jdb_accounting),
#if defined(CONFIG_MP) || defined(CONFIG_AMP)
    Max_num_cpus = CONFIG_MP_MAX_CPUS,
#else
    Max_num_cpus = 1,
#endif
    Big_endian = TAG_ENABLED(big_endian),
    Have_mmu = TAG_ENABLED(mmu),
    Have_mpu = !TAG_ENABLED(mmu),
  };

  static Cpu_number max_num_cpus() { return Cpu_number(Max_num_cpus); }

  static Global_data<bool> getchar_does_hlt_works_ok;
  static Global_data<bool> esc_hack;
  static Global_data<unsigned> tbuf_entries;

  static constexpr Order page_order()
  { return Order(PAGE_SHIFT); }

  static constexpr Bytes page_size()
  { return Bytes(PAGE_SIZE); }

  static constexpr unsigned kmem_per_cent();
  static constexpr unsigned long kmem_max();

  static constexpr unsigned long ext_vcpu_size();
};

#define GREETING_COLOR_ANSI_TITLE  "\033[1;32m"
#define GREETING_COLOR_ANSI_INFO   "\033[0;32m"

INTERFACE[ia32 || amd64]:
#define DISPLAY_ARCH "x86"

INTERFACE[!(ia32 || amd64)]:
#define DISPLAY_ARCH CONFIG_XARCH

INTERFACE[bit64]:
#define TARGET_WORD_LEN "64"

INTERFACE[!bit64]:
#define TARGET_WORD_LEN "32"

INTERFACE:

#define CONFIG_KERNEL_VERSION_STRING \
  GREETING_COLOR_ANSI_TITLE "Welcome to the L4Re Microkernel!\\n"         \
  GREETING_COLOR_ANSI_INFO "L4Re Microkernel on " DISPLAY_ARCH "-" TARGET_WORD_LEN "\\n"      \
                           "Rev: " CODE_VERSION " compiled with " COMPILER \
                           TARGET_NAME_PHRASE "    [" CONFIG_LABEL "]\\n"    \
                           "Build: #" BUILD_NR " " BUILD_DATE "\\n"            \
  GREETING_COLOR_ANSI_OFF

//---------------------------------------------------------------------------
INTERFACE [serial]:

EXTENSION class Config
{
public:
  static Global_data<int>  serial_esc;
};

//---------------------------------------------------------------------------
INTERFACE [!serial]:

EXTENSION class Config
{
public:
  static const int serial_esc = 0;
};

//---------------------------------------------------------------------------
INTERFACE [!arm]:

EXTENSION class Config
{
public:
  // Attention: this enum is used by the Lauterbach Trace32 OS awareness.
  enum Ext_vcpu_info : Mword
  {
    Ext_vcpu_infos_offset = 0x200,
    Ext_vcpu_state_offset = 0x400,
  };
};

//---------------------------------------------------------------------------
INTERFACE [!virtual_space_iface]:

#define FIASCO_SPACE_VIRTUAL

//---------------------------------------------------------------------------
INTERFACE [virtual_space_iface]:

#define FIASCO_SPACE_VIRTUAL virtual

//---------------------------------------------------------------------------
INTERFACE [!virt_obj_space]:

#define FIASCO_VIRT_OBJ_SPACE_OVERRIDE

//---------------------------------------------------------------------------
INTERFACE [virt_obj_space]:

#define FIASCO_VIRT_OBJ_SPACE_OVERRIDE override


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "feature.h"
#include "koptions.h"
#include "panic.h"
#include "std_macros.h"

KIP_KERNEL_ABI_VERSION(FIASCO_STRINGIFY(FIASCO_KERNEL_SUBVERSION));

// class variables
DEFINE_GLOBAL Global_data<bool> Config::esc_hack;
#ifdef CONFIG_SERIAL
DEFINE_GLOBAL_CONSTINIT
Global_data<int>  Config::serial_esc(Config::SERIAL_NO_ESC);
#endif

#ifdef CONFIG_MMU
DEFINE_GLOBAL_CONSTINIT
Global_data<unsigned> Config::tbuf_entries(0x20000 / sizeof(Mword)); //1024;
#else
// The Buddy_alloc can only allocate up to 128KiB. So far only Arm is no-MMU
// capable and we know the size of each tbuf entry.
DEFINE_GLOBAL_CONSTINIT
Global_data<unsigned> Config::tbuf_entries((1U << 17) / (sizeof(Mword) * 16));
#endif

DEFINE_GLOBAL Global_data<bool> Config::getchar_does_hlt_works_ok;

IMPLEMENT FIASCO_INIT
void Config::init()
{
  init_arch();

  if (Koptions::o()->opt(Koptions::F_esc))
    esc_hack = true;

#ifdef CONFIG_SERIAL
  if (    Koptions::o()->opt(Koptions::F_serial_esc)
      && !Koptions::o()->opt(Koptions::F_noserial)
      && !Koptions::o()->opt(Koptions::F_nojdb))
    {
      serial_esc = SERIAL_ESC_IRQ;
    }
#endif
}

PUBLIC static
unsigned long
Config::kmem_size([[maybe_unused]] unsigned long available_size)
{
#ifdef CONFIG_KMEM_SIZE_AUTO
  static_assert(kmem_per_cent() < 100, "Sanitize kmem_per_cent");
  unsigned long alloc_size = available_size / 100U * kmem_per_cent();
  if (alloc_size > kmem_max())
    alloc_size = kmem_max();
  return alloc_size;
#else
  return static_cast<unsigned long>(CONFIG_KMEM_SIZE_KB) << 10;
#endif
}

IMPLEMENT_DEFAULT
constexpr unsigned long Config::ext_vcpu_size() { return PAGE_SIZE; }

//---------------------------------------------------------------------------
IMPLEMENTATION [!64bit]:

IMPLEMENT_DEFAULT inline ALWAYS_INLINE
constexpr unsigned Config::kmem_per_cent() { return 8; }

IMPLEMENT_DEFAULT inline ALWAYS_INLINE
constexpr unsigned long Config::kmem_max() { return 60UL << 20; }

//---------------------------------------------------------------------------
IMPLEMENTATION [64bit]:

IMPLEMENT_DEFAULT inline ALWAYS_INLINE
constexpr unsigned Config::kmem_per_cent() { return 6; }

IMPLEMENT_DEFAULT inline ALWAYS_INLINE
constexpr unsigned long Config::kmem_max() { return 3328UL << 20; }

//---------------------------------------------------------------------------
IMPLEMENTATION [fine_grained_cputime]:

KIP_KERNEL_FEATURE("fi_gr_cputime");

//---------------------------------------------------------------------------
IMPLEMENTATION [mapdb]:

KIP_KERNEL_FEATURE("mapdb");

//---------------------------------------------------------------------------
IMPLEMENTATION [perf_cnt_user]:

KIP_KERNEL_FEATURE("perf_cnt_user");
