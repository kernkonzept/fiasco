INTERFACE:

EXTENSION class Kernel_thread
{
public:
  static int	init_done();

private:
  static int	free_initcall_section_done;
};

IMPLEMENTATION[ux]:

#include <unistd.h>
#include <sys/mman.h>
#include "fb.h"
#include "kdb_ke.h"
#include "koptions.h"
#include "net.h"
#include "mem_layout.h"
#include "pic.h"
#include "trap_state.h"
#include "usermode.h"

int Kernel_thread::free_initcall_section_done;

IMPLEMENT inline
int
Kernel_thread::init_done()
{
  return free_initcall_section_done;
}

IMPLEMENT inline NEEDS [<unistd.h>, <sys/mman.h>, "mem_layout.h"]
void
Kernel_thread::free_initcall_section()
{
  munmap((void*)&Mem_layout::initcall_start,
         &Mem_layout::initcall_end - &Mem_layout::initcall_start);
  free_initcall_section_done = 1;
}

static void ux_termination_handler()
{
  puts ("\nExiting, wait...");

  Helping_lock::threading_system_active = false;
  signal (SIGIO, SIG_IGN);		// Ignore hardware interrupts
  // This is a great hack to delete the processes on fiasco UX
  extern char _boot_sys_end[];
  ::Kobject_dbg::Iterator c = ::Kobject_dbg::_kobjects.begin();
  while (c != ::Kobject_dbg::_kobjects.end())
    {
      ::Kobject_dbg::Iterator n = c;
      ++n;
      Kobject *o = Kobject::from_dbg(c);
      if ((void*)o > (void*)_boot_sys_end)
	{
	  // printf("Zapp: %p (%s)\n", o, o->kobj_type());
	  delete o;
	}
      c = n;
    }

  fflush(0);  // Flush output stream
}


IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap_arch()
{
  // install slow trap handler
  nested_trap_handler      = Trap_state::base_handler;
  Trap_state::base_handler = thread_handle_trap;

  if (Koptions::o()->opt(Koptions::F_jdb_cmd))
    kdb_ke_sequence(Koptions::o()->jdb_cmd);

  atexit(ux_termination_handler);
  boot_app_cpus();
}

//--------------------------------------------------------------------------
IMPLEMENTATION [!mp]:

PUBLIC
static inline void
Kernel_thread::boot_app_cpus()
{}


//--------------------------------------------------------------------------
IMPLEMENTATION [mp]:

PUBLIC
static void
Kernel_thread::boot_app_cpus()
{
  printf("MP: launching APs...\n");
  if (0)
    {
       extern char _tramp_mp_entry[];
	printf("new child: %d   [%d]\n",
	       Emulation::spawn_cpu_thread((Address)_tramp_mp_entry),
	       Emulation::gettid());
    }
}
