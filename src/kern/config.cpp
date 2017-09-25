/*
 * Global kernel configuration
 */

INTERFACE:

#include <globalconfig.h>
#include "config_tcbsize.h"
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

    Kip_syscalls = 1,

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
    Mapdb_ram_only = 0,
#ifdef CONFIG_DEBUG_KERNEL_PAGE_FAULTS
    Log_kernel_page_faults = 1,
#else
    Log_kernel_page_faults = 0,
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
  };

  static Cpu_number max_num_cpus() { return Cpu_number(Max_num_cpus); }

  static bool getchar_does_hlt_works_ok;
  static bool esc_hack;
  static unsigned tbuf_entries;
  static unsigned num_ap_cpus asm("config_num_ap_cpus");
};

#define GREETING_COLOR_ANSI_TITLE  "\033[1;32m"
#define GREETING_COLOR_ANSI_INFO   "\033[0;32m"

INTERFACE[ia32]:
#define TARGET_NAME CONFIG_IA32_TARGET

INTERFACE[ux]:
#define TARGET_NAME CONFIG_IA32_TARGET

INTERFACE[amd64]:
#define TARGET_NAME CONFIG_IA32_TARGET

INTERFACE:

#define CONFIG_KERNEL_VERSION_STRING \
  GREETING_COLOR_ANSI_TITLE "Welcome to L4/Fiasco.OC!\\n"                      \
  GREETING_COLOR_ANSI_INFO "L4/Fiasco.OC microkernel on " CONFIG_XARCH "\\n"      \
                           "Rev: " CODE_VERSION " compiled with " COMPILER \
                           TARGET_NAME_PHRASE "    [" CONFIG_LABEL "]\\n"    \
                           "Build: #" BUILD_NR " " BUILD_DATE "\\n"            \
  GREETING_COLOR_ANSI_OFF

//---------------------------------------------------------------------------
INTERFACE [ux]:

EXTENSION class Config
{
public:
  // 32MB RAM => 2.5MB kmem, 128MB RAM => 16MB kmem, >=512MB RAM => 64MB kmem
  static const unsigned kernel_mem_per_cent = 8;
  enum
  {
    kernel_mem_max      = 64 << 20
  };
};

//---------------------------------------------------------------------------
INTERFACE [!ux && !64bit]:

EXTENSION class Config
{
public:
  // 32MB RAM => 2.5MB kmem, 128MB RAM => 16MB kmem, >=512MB RAM => 60MB kmem
  static const unsigned kernel_mem_per_cent = 8;
  enum
  {
    kernel_mem_max      = 60 << 20
  };
};

//---------------------------------------------------------------------------
INTERFACE [!ux && 64bit]:

EXTENSION class Config
{
public:
  // 32MB RAM => 2.5MB kmem, 128MB RAM => 16MB kmem, >=512MB RAM => 60MB kmem
  static const unsigned kernel_mem_per_cent = 6;
  enum
  {
    kernel_mem_max      = 1024 << 20
  };
};

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
IMPLEMENTATION:

#include <cstring>
#include <cstdlib>
#include "feature.h"
#include "initcalls.h"
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
unsigned Config::num_ap_cpus;

#ifdef CONFIG_FINE_GRAINED_CPUTIME
KIP_KERNEL_FEATURE("fi_gr_cputime");
#endif

//-----------------------------------------------------------------------------
IMPLEMENTATION:

IMPLEMENT FIASCO_INIT
void Config::init()
{
  init_arch();

  if (Koptions::o()->opt(Koptions::F_esc))
    esc_hack = true;

#ifdef CONFIG_SERIAL
  if (    Koptions::o()->opt(Koptions::F_serial_esc)
      && !Koptions::o()->opt(Koptions::F_noserial)
# ifdef CONFIG_KDB
      &&  Koptions::o()->opt(Koptions::F_nokdb)
# endif
      && !Koptions::o()->opt(Koptions::F_nojdb))
    {
      serial_esc = SERIAL_ESC_IRQ;
    }
#endif
}

