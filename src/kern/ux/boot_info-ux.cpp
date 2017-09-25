/*
 * Fiasco-UX
 * Architecture specific bootinfo code
 */

INTERFACE:

#include <sys/types.h>                  // for pid_t
#include <getopt.h>                     // for struct option
#include "l4_types.h"                   // for Global_id
#include "multiboot.h"

EXTENSION class Boot_info
{
public:
  static Address mbi_phys();
  static Multiboot_info *mbi_virt();

private:
  static int                            _fd;            // Physmem FD
  static pid_t                          _pid;           // Process ID
  static char **                        _args;          // Cmd Line Parameters
  static bool                           _irq0_disabled;
  static unsigned long                  _input_size;
  static unsigned long                  _fb_size;
  static Address                        _fb_virt;
  static Address                        _fb_phys;
  static unsigned int                   _fb_width;
  static unsigned int                   _fb_height;
  static unsigned int                   _fb_depth;
  static const char *                   _fb_program;
  static bool                           _net;
  static const char *                   _net_program;
  static void *                         _mbi_vbe;
  static const char *                   _irq0_program;
  static struct option                  _long_options[];
  static char                           _help[];
  static char const *                   _modules[];
  static bool				_emulate_clisti;
  static unsigned long                  _sigma0_start;
  static unsigned long                  _sigma0_end;
  static unsigned long                  _root_start;
  static unsigned long                  _root_end;
  static unsigned long                  _min_mappable_address;
};

IMPLEMENTATION[ux]:

// for our init code we do not care about the stack size
#pragma GCC diagnostic ignored "-Wframe-larger-than="

#include <cassert>                      // for assert
#include <cerrno>                       // for errno
#include <climits>                      // for CHAR_BIT
#include <cstdlib>                      // for atol
#include <cstring>                      // for stpcpy
#include <cstdio>                       // for printf
#include <fcntl.h>                      // for open
#include <panic.h>                      // for panic
#include <unistd.h>                     // for getopt
#include <sys/mman.h>                   // for mmap
#include <sys/stat.h>                   // for open
#include <sys/statvfs.h>
#include <sys/utsname.h>                // for uname
#include <sys/resource.h>

#include "config.h"
#include "emulation.h"
#include "initcalls.h"
#include "kernel_console.h"
#include "koptions.h"
#include "loader.h"
#include "mem_layout.h"

int                     Boot_info::_fd;
pid_t                   Boot_info::_pid;
char **                 Boot_info::_args;
bool                    Boot_info::_irq0_disabled;
unsigned long           Boot_info::_input_size;
unsigned long           Boot_info::_fb_size;
Address                 Boot_info::_fb_virt = 0xc0000000;
Address                 Boot_info::_fb_phys;
unsigned int            Boot_info::_fb_width;
unsigned int            Boot_info::_fb_height;
unsigned int            Boot_info::_fb_depth;
const char *            Boot_info::_fb_program = "ux_con";
bool                    Boot_info::_net;
const char *            Boot_info::_net_program = "ux_net";
void *                  Boot_info::_mbi_vbe;
const char *            Boot_info::_irq0_program = "irq0";
bool			Boot_info::_emulate_clisti;
unsigned long           Boot_info::_sigma0_start;
unsigned long           Boot_info::_sigma0_end;
unsigned long           Boot_info::_root_start;
unsigned long           Boot_info::_root_end;
unsigned long           Boot_info::_min_mappable_address;


// If you add options here, add them to getopt_long and help below
struct option Boot_info::_long_options[] FIASCO_INITDATA =
{
  { "physmem_file",             required_argument,      NULL, 'f' },
  { "help",                     no_argument,            NULL, 'h' },
  { "jdb_cmd",                  required_argument,      NULL, 'j' },
  { "kmemsize",                 required_argument,      NULL, 'k' },
  { "load_module",              required_argument,      NULL, 'l' },
  { "memsize",                  required_argument,      NULL, 'm' },
  { "native_task",              required_argument,      NULL, 'n' },
  { "quiet",                    no_argument,            NULL, 'q' },
  { "tbuf_entries",             required_argument,      NULL, 't' },
  { "wait",                     no_argument,            NULL, 'w' },
  { "fb_program",               required_argument,      NULL, 'F' },
  { "fb_geometry",              required_argument,      NULL, 'G' },
  { "net",                      no_argument,            NULL, 'N' },
  { "net_program",              required_argument,      NULL, 'E' },
  { "irq0",                     required_argument,      NULL, 'I' },
  { "roottask",                 required_argument,      NULL, 'R' },
  { "sigma0",                   required_argument,      NULL, 'S' },
  { "test",                     no_argument,            NULL, 'T' },
  { "disable_irq0",             no_argument,            NULL, '0' },
  { "symbols",                  required_argument,      NULL, 'Y' },
  { "roottaskconfig",           required_argument,      NULL, 'C' },
  { "lines",                    required_argument,      NULL, 'L' },
  { "clisti",			no_argument,		NULL, 's' },
  { 0, 0, 0, 0 }
};

// Keep this in sync with the options above
char Boot_info::_help[] FIASCO_INITDATA =
  "-f file     : Specify the location of the physical memory backing store\n"
  "-l module   : Specify a module\n"
  "-j jdb cmds : Specify non-interactive Jdb commands to be executed at startup\n"
  "-k kmemsize : Specify size in KiB to be reserved for kernel\n"
  "-m memsize  : Specify physical memory size in MB (currently up to 1024)\n"
  "-q          : Suppress any startup message\n"
  "-t number   : Specify the number of trace buffer entries (up to 32768)\n"
  "-w          : Enter kernel debugger on startup and wait\n"
  "-0          : Disable the timer interrupt generator\n"
  "-F          : Specify a different frame buffer program\n"
  "-G          : Geometry for frame buffer: widthxheight@depth\n"
  "-N          : Enable network support\n"
  "-E          : Specify net helper binary\n"
  "-I          : Specify different irq0 binary\n"
  "-R          : Specify different roottask binary\n"
  "-S          : Specify different sigma0 binary\n"
  "-Y          : Specify symbols path\n"
  "-L          : Specify lines path\n"
  "-C          : Specify roottask configuration file path\n"
  "-T          : Test mode -- do not load any modules\n"
  "-s          : Emulate cli/sti instructions\n";


char const *Boot_info::_modules[64] FIASCO_INITDATA =
{
  "fiasco",
  "sigma0",
  "moe",
   NULL,        // Symbols
   NULL,        // Lines
   NULL         // Roottask configuration
};


PUBLIC static
Address
Boot_info::kmem_start(Address mem_max)
{
  Address end_addr = (mbi_virt()->mem_upper + 1024) << 10;
  Address size, base;

  if (end_addr > mem_max)
    end_addr = mem_max;

  size = Koptions::o()->kmemsize << 10;
  if (!size)
    {
      size = end_addr / 100 * Config::kernel_mem_per_cent;
      if (size > Config::kernel_mem_max)
	size = Config::kernel_mem_max;
    }

  base = (end_addr - size) & Config::PAGE_MASK;
  if (Mem_layout::phys_to_pmem(base) < Mem_layout::Physmem)
    base = Mem_layout::pmem_to_phys(Mem_layout::Physmem);

  return base;
}



IMPLEMENT FIASCO_INIT
void
Boot_info::init()
{
  extern int                    __libc_argc;
  extern char **                __libc_argv;
  register unsigned long        _sp asm("esp");
  char const                    *physmem_file = "/tmp/physmem";
  char const                    *error, **m;
  char                          *str, buffer[4096];
  int                           arg;
  bool                          quiet = false;
  unsigned int                  modcount = 6, skip = 3;
  unsigned long int             memsize = 64 << 20;
  Multiboot_info *              mbi;
  Multiboot_module *            mbm;
  struct utsname                uts;
#if 0
  char const *                  symbols_file = "Symbols";
  char const *                  lines_file = "Lines";
  char const *                  roottask_config_file = "roottask.config";
#endif

  _args = __libc_argv;
  _pid  = getpid ();

  *(str = buffer) = 0;

  // Parse command line. Use getopt_long_only() to achieve more compatibility
  // with command line switches in the IA32 architecture.
  while ((arg = getopt_long_only (__libc_argc, __libc_argv,
                                  "C:f:hj:k:l:m:qst:wE:F:G:I:L:NR:S:TY:0",
                                  _long_options, NULL)) != -1) {
    switch (arg) {

      case 'S':
        _modules[1] = optarg;
        break;

      case 'R':
        _modules[2] = optarg;
        break;

      case 'Y': // sYmbols path
#if 0
        symbols_file = optarg;
#endif
        break;

      case 'L': // Lines path
#if 0
        lines_file = optarg;
#endif
        break;

      case 'C': // roottask Configuration file
#if 0
	roottask_config_file = optarg;
#endif
	break;

      case 'l':
        if (modcount < sizeof (_modules) / sizeof (*_modules))
          _modules[modcount++] = optarg;
	else
	  printf("WARNING: max modules exceeded (dropped '%s')\n",
	      optarg);
        break;

      case 'f':
        physmem_file = optarg;
        break;

      case 'j':
	set_jdb_cmd(optarg);
        break;

      case 'k':
        kmemsize(atol(optarg) << 10);
        break;

      case 'm':
        memsize = atol (optarg) << 20;
        break;

      case 'q':
        quiet = true;
        Kconsole::console()->change_state(0, 0, ~Console::OUTENABLED, 0);
        break;

      case 't':
        Config::tbuf_entries = atol (optarg);
        break;

      case 'w':
        set_wait();
        break;

      case '0':
        _irq0_disabled = true;
        break;

      case 'F':
        _fb_program = optarg;
        break;

      case 'G':
        if (sscanf (optarg, "%ux%u@%u", &_fb_width, &_fb_height, &_fb_depth)
            == 3)
          {
            _fb_size = _fb_width * _fb_height * ((_fb_depth + 7) >> 3);
            _fb_size += ~Config::SUPERPAGE_MASK;
            _fb_size &=  Config::SUPERPAGE_MASK;        // Round up to 4 MB

            _input_size  = Config::PAGE_SIZE;
          }
        break;

      case 'N':
        _net = 1;
        break;

      case 'E':
        _net_program = optarg;
        break;

      case 'I':
        _irq0_program = optarg;
        break;

      case 'T':
        modcount = 0; skip = 0;
        break;

      case 's':
	_emulate_clisti = 1;
	break;

      default:
        printf ("Usage: %s\n\n%s", *__libc_argv, _help);
        exit (arg == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  }

  if (_sp < ((Mem_layout::Host_as_base - 1) & Config::SUPERPAGE_MASK)
      && munmap((void *)((Mem_layout::Host_as_base - 1) & Config::PAGE_MASK),
	        Config::PAGE_SIZE) == -1 && errno == EINVAL)
    {
      printf(" Fiasco-UX does only run with %dGB user address space size.\n"
             " Please make sure your host Linux kernel is configured for %dGB\n"
	     " user address space size. Check for\n"
	     "     CONFIG_PAGE_OFFSET=0x%X\n"
	     " in your host Linux .config.\n",
	     Mem_layout::Host_as_base >> 30, Mem_layout::Host_as_base >> 30,
	     Mem_layout::Host_as_base);
      exit(EXIT_FAILURE);
    }

  // Check roottask command line for symbols and lines options
#if 0
  if (strstr (_modules[2], " -symbols"))
    {
      _modules[3] = symbols_file;
      skip--;
    }
  if (strstr (_modules[2], " -lines"))
    {
      _modules[4] = lines_file;
      skip--;
    }
  if (strstr (_modules[2], " -configfile"))
    {
      _modules[5] = roottask_config_file;
      skip--;
    }
#endif

  if (uname (&uts))
    panic("uname: %s", strerror (errno));

  if (! quiet)
    printf ("\n\nFiasco-UX on %s %s (%s)\n",
            uts.sysname, uts.release, uts.machine);

  struct rlimit rl;
  if (getrlimit(RLIMIT_STACK, &rl))
    perror("getrlimit");
  else
    {
      rl.rlim_max = rl.rlim_cur = 1 << 17;
      if (setrlimit(RLIMIT_STACK, &rl))
	perror("setrlimit");
    }

  get_minimum_map_address();
  find_x86_tls_base();

  if ((_fd = open (physmem_file, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0700))
      == -1)
    panic ("cannot create physmem: %s", strerror (errno));

  // check if we really got exec-rights on physmem, /tmp might be mounted noexec
  if (access(physmem_file, R_OK | W_OK | X_OK))
    {
      unlink (physmem_file);
      panic ("Invalid permissions for physmem file");
    }

  struct statvfs stvfs;
  if (statvfs(physmem_file, &stvfs) < 0)
    {
      perror("statvfs");
      panic("Cannot query statistics on physmem file");
    }

  unlink (physmem_file);

  // Cannot handle more physical memory than roottask can handle (currently 1 GB).
  // roottask considers anything greater than 0x40000000 as adapter page.
#ifdef CONFIG_CONTEXT_4K
  // With 4k kernel thread context size we cannot handle more than 512MB
  // physical memory (mapped to the virtual area 0x90000000..0xb0000000).
  if (memsize > 1 << 29)
    memsize = 1 << 29;
#else
  if (memsize > 1 << 30)
    memsize = 1 << 30;
#endif

  if (memsize + _fb_size + _input_size > stvfs.f_bavail * stvfs.f_bsize)
    {
      printf("WARNING: Available free space of %ldMB for '%s' might not be enough.\n",
             (stvfs.f_bavail * stvfs.f_bsize) >> 20, physmem_file);
    }

  // The framebuffer starts beyond the physical memory
  _fb_phys = memsize;

  // Create a sparse file as backing store
  if (lseek (_fd, memsize + _fb_size + _input_size - 1, SEEK_SET) == -1 ||
      write (_fd, "~", 1) != 1)
    panic ("cannot resize physmem: %s", strerror (errno));

  // Now map the beast in our virtual address space
  if (mmap (reinterpret_cast<void *>(Mem_layout::Physmem),
            memsize + _fb_size + _input_size,
            PROT_READ | PROT_WRITE,
            MAP_FIXED | MAP_SHARED, _fd, 0) == MAP_FAILED)
    panic ("cannot mmap physmem: %s", strerror (errno));

  if (! quiet)
    printf ("Mapped %lu MB Memory + %lu KB Framebuffer + "
            "%lu KB Input Area on FD %d\n\n",
            memsize >> 20, _fb_size >> 10, _input_size >> 10, _fd);

  mbi             = mbi_virt();
  mbi->flags      = Multiboot_info::Memory | Multiboot_info::Cmdline |
                    Multiboot_info::Mods;
  mbi->mem_lower  = 0;
  mbi->mem_upper  = (memsize >> 10) - 1024;            /* in KB */
  mbi->mods_count = modcount - skip;
  mbi->mods_addr  = mbi_phys() + sizeof (*mbi);
  mbm = reinterpret_cast<Multiboot_module *>((char *) mbi + sizeof (*mbi));
  str = reinterpret_cast<char *>(mbm + modcount - skip);

  // Copying of modules starts at the top, right below the kmem reserved area

  // Skip an area of 1MB in order to allow sigma0 to allocate its memmap
  // structures at top of physical memory. Note that the space the boot
  // modules are loaded at is later freed in most cases so we prevent memory
  // fragmentation here.
  Address load_addr = kmem_start (0xffffffff) - (1 << 20);


  // Load/copy the modules
  for (m = _modules; m < _modules + modcount; m++)
    {
      unsigned long start, end;
      char *cmd;

      if (!*m)
        continue;

      // Cut off between module name and command line
      if ((cmd = strchr (const_cast<char *>(*m), ' ')))
        *cmd = 0;

      if (m == _modules)
	{
	  mbm->mod_start = 0;
	  mbm->mod_end = 0;
	  error = 0;
	}
      else
	// Load sigma0 and roottask, just copy the rest
	error = m < _modules + 3 ?
              Loader::load_module (*m, mbm, memsize, quiet, &start, &end) :
              Loader::copy_module (*m, mbm, &load_addr, quiet);

      if (error)
        {
          printf ("%s: %s\n", error, *m);
          exit (EXIT_FAILURE);
        }

      // Reattach command line
      if (cmd)
        *cmd = ' ';

      mbm->string = str - (char *) mbi + mbi_phys();

      // Copy module name with path and command line
      str = stpcpy (str, *m) + 1;

      // sigma0: save memory region
      if (m == _modules + 1)
        {
	  _sigma0_start = start;
	  _sigma0_end   = end;
	}
      else
      // roottask: save memory region and pass in the extra command line
      if (m == _modules + 2)
        {
	  _root_start = start;
	  _root_end   = end;
          str--;
          str = stpcpy (str, buffer) + 1;
        }

      mbm++;
    }
  _mbi_vbe = str;

  if (! quiet)
    puts ("\nBootstrapping...");
}

PRIVATE static
void
Boot_info::get_minimum_map_address()
{
  FILE *f = fopen("/proc/sys/vm/mmap_min_addr", "r");
  if (!f)
    return;

  int r = fscanf(f, "%ld", &_min_mappable_address);
  fclose(f);

  // funny thing is that although /proc/sys/vm/mmap_min_addr might be
  // readable according to the file permissions it might not actually work
  // (a read returns EPERM, seen with 2.6.33), since at at least Ubuntu sets
  // mmap_min_addr to 0x10000 we take this as the default value here
  if (r == -1)
    _min_mappable_address = 0x10000;

  _min_mappable_address
    = (_min_mappable_address + (Config::PAGE_SIZE - 1)) & Config::PAGE_MASK;
}

PRIVATE static
void
Boot_info::find_x86_tls_base()
{
  // the base is different on 32 and 64 bit hosts, so we need to find the
  // right one
  // be very pessimistic and scan a larger range

  for (unsigned nr = 0; nr < 40; ++nr)
    {
      Ldt_user_desc desc;
      int r = Emulation::get_thread_area(&desc, nr);
      if (r == -38)
	panic("get_thread_area not available");
      if (!r)
        {
	  Emulation::set_host_tls_base(nr);
	  return;
        }
    }

  panic("finding TLS base did not work");
}

PUBLIC static inline
void
Boot_info::reset_checksum_ro()
{}

IMPLEMENT inline NEEDS ["mem_layout.h"]
Address
Boot_info::mbi_phys()
{
  return Mem_layout::Multiboot_frame;
}

IMPLEMENT inline NEEDS ["mem_layout.h"]
Multiboot_info *
Boot_info::mbi_virt()
{
  return reinterpret_cast<Multiboot_info * const>
    (Mem_layout::Physmem + mbi_phys());
}

PUBLIC static inline
Multiboot_vbe_controller *
Boot_info::mbi_vbe()
{
  return reinterpret_cast<Multiboot_vbe_controller *>(_mbi_vbe);
}

PUBLIC static inline
unsigned long
Boot_info::mbi_size()
{
  return (unsigned long)mbi_vbe()
    + sizeof (Multiboot_vbe_controller) + sizeof (Multiboot_vbe_mode)
    - (unsigned long)mbi_virt();
}

PUBLIC static inline
int
Boot_info::fd()
{ return _fd; }

PUBLIC static inline
pid_t
Boot_info::pid()
{ return _pid; }

PUBLIC static inline
char **
Boot_info::args()
{ return _args; }

PUBLIC static inline
bool
Boot_info::irq0_disabled()
{ return _irq0_disabled; }

PUBLIC static inline
unsigned long
Boot_info::input_start()
{ return fb_virt() + fb_size(); }

PUBLIC static inline
unsigned long
Boot_info::input_size()
{ return _input_size; }

PUBLIC static inline
unsigned long
Boot_info::fb_size()
{ return _fb_size; }

PUBLIC static inline
Address
Boot_info::fb_virt()
{ return _fb_virt; }

PUBLIC static inline
Address
Boot_info::fb_phys()
{ return _fb_phys; }

PUBLIC static inline
unsigned int
Boot_info::fb_width()
{ return _fb_width; }

PUBLIC static inline
unsigned int
Boot_info::fb_height()
{ return _fb_height; }

PUBLIC static inline
unsigned int
Boot_info::fb_depth()
{ return _fb_depth; }

PUBLIC static inline
const char *
Boot_info::fb_program()
{ return _fb_program; }

PUBLIC static inline
const char *
Boot_info::net_program()
{ return _net_program; }

PUBLIC static inline
bool
Boot_info::net()
{ return _net; }

PUBLIC static inline
const char *
Boot_info::irq0_path()
{ return _irq0_program; }

PUBLIC static inline
bool
Boot_info::emulate_clisti()
{ return _emulate_clisti; }

PUBLIC static inline
unsigned long
Boot_info::sigma0_start()
{ return _sigma0_start; }

PUBLIC static inline
unsigned long
Boot_info::sigma0_end()
{ return _sigma0_end; }

PUBLIC static inline
unsigned long
Boot_info::root_start()
{ return _root_start; }

PUBLIC static inline
unsigned long
Boot_info::root_end()
{ return _root_end; }

PUBLIC static inline
unsigned long
Boot_info::min_mappable_address()
{ return _min_mappable_address; }

PUBLIC inline static
unsigned
Boot_info::get_checksum_ro(void)
{ return 0; }

PUBLIC inline static
unsigned
Boot_info::get_checksum_rw(void)
{ return 0; }


PUBLIC static
void
Boot_info::set_wait()
{ Koptions::o()->flags |= Koptions::F_wait; }

PUBLIC static
void
Boot_info::kmemsize(unsigned int k)
{ Koptions::o()->kmemsize = k; }

PUBLIC static
void
Boot_info::set_jdb_cmd(const char *j)
{
  strncpy(Koptions::o()->jdb_cmd, j, sizeof(Koptions::o()->jdb_cmd));
  Koptions::o()->jdb_cmd[sizeof(Koptions::o()->jdb_cmd) - 1] = 0;
}

