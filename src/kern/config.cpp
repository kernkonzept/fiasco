/*
 * Global kernel configuration
 */

INTERFACE:

#include <globalconfig.h>
#include "config_tcbsize.h"
#include "initcalls.h"
#include "l4_types.h"

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
    // kernel (idle) thread prio
    Kernel_prio = 0,
    // default prio
    Default_prio = 1,

    Warn_level = CONFIG_WARN_LEVEL,

    One_shot_min_interval_us =   200,
    One_shot_max_interval_us = 10000,


#ifdef CONFIG_FINE_GRAINED_CPUTIME
    Fine_grained_cputime = true,
#else
    Fine_grained_cputime = false,
#endif

#ifdef CONFIG_STACK_DEPTH
    Stack_depth = true,
#else
    Stack_depth = false,
#endif
#ifdef CONFIG_NO_FRAME_PTR
    Have_frame_ptr = 0,
#else
    Have_frame_ptr = 1,
#endif
#ifdef CONFIG_JDB
    Jdb = 1,
#else
    Jdb = 0,
#endif
#ifdef CONFIG_JDB_LOGGING
    Jdb_logging = 1,
#else
    Jdb_logging = 0,
#endif
#ifdef CONFIG_JDB_ACCOUNTING
    Jdb_accounting = 1,
#else
    Jdb_accounting = 0,
#endif
#ifdef CONFIG_MP
    Max_num_cpus = CONFIG_MP_MAX_CPUS,
#else
    Max_num_cpus = 1,
#endif
#ifdef CONFIG_BIG_ENDIAN
    Big_endian = true,
#else
    Big_endian = false,
#endif
  };

  static Cpu_number max_num_cpus() { return Cpu_number(Max_num_cpus); }

  static bool getchar_does_hlt_works_ok;
  static bool esc_hack;
  static unsigned tbuf_entries;

  static constexpr Order page_order()
  { return Order(PAGE_SHIFT); }

  static constexpr Bytes page_size()
  { return Bytes(PAGE_SIZE); }

  static constexpr unsigned kmem_per_cent();
  static constexpr unsigned long kmem_max();
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
  static int  serial_esc;
};

//---------------------------------------------------------------------------
INTERFACE [!serial]:

EXTENSION class Config
{
public:
  static const int serial_esc = 0;
};


//---------------------------------------------------------------------------
INTERFACE [!virtual_space_iface]:

#define FIASCO_SPACE_VIRTUAL

//---------------------------------------------------------------------------
INTERFACE [virtual_space_iface]:

#define FIASCO_SPACE_VIRTUAL virtual

//---------------------------------------------------------------------------
INTERFACE [!virt_obj_space || ux]:

#define FIASCO_VIRT_OBJ_SPACE_OVERRIDE

//---------------------------------------------------------------------------
INTERFACE [virt_obj_space && !ux]:

#define FIASCO_VIRT_OBJ_SPACE_OVERRIDE override


//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <cstring>
#include <cstdlib>
#include "feature.h"
#include "koptions.h"
#include "panic.h"
#include "std_macros.h"

KIP_KERNEL_ABI_VERSION(FIASCO_STRINGIFY(FIASCO_KERNEL_SUBVERSION));

// class variables
bool Config::esc_hack = false;
#ifdef CONFIG_SERIAL
int  Config::serial_esc = Config::SERIAL_NO_ESC;
#endif

unsigned Config::tbuf_entries = 0x20000 / sizeof(Mword); //1024;
bool Config::getchar_does_hlt_works_ok = false;

#ifdef CONFIG_FINE_GRAINED_CPUTIME
KIP_KERNEL_FEATURE("fi_gr_cputime");
#endif
#ifdef CONFIG_MAPDB
KIP_KERNEL_FEATURE("mapdb");
#endif

//-----------------------------------------------------------------------------
IMPLEMENTATION:

#include <stdio.h>

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

PUBLIC static FIASCO_INIT
unsigned long
Config::kmem_size(unsigned long available_size)
{
#ifdef CONFIG_KMEM_SIZE_AUTO
  static_assert(kmem_per_cent() < 100, "Sanitize kmem_per_cent");
  unsigned long alloc_size = available_size / 100U * kmem_per_cent();
  if (alloc_size > kmem_max())
    alloc_size = kmem_max();
  return alloc_size;
#else
  (void)available_size;
  return (unsigned long)CONFIG_KMEM_SIZE_KB << 10;
#endif
}

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
