INTERFACE [ux]:

#include "types.h"
#include "l4_types.h"

class Trap_state;
class Thread;
class Context;
class Jdb_entry_frame;
class Mem_space;

EXTENSION class Jdb
{
public:
  typedef enum
    {
      s_unknown, s_ipc, s_syscall, s_pagefault, s_fputrap,
      s_interrupt, s_timer_interrupt, s_slowtrap, s_user_invoke,
    } Guessed_thread_state;

  enum { MIN_SCREEN_HEIGHT = 20, MIN_SCREEN_WIDTH = 80 };

  template < typename T > static T peek(T const *addr, Address_type user);

  static int (*bp_test_log_only)();
  static int (*bp_test_break)(String_buffer *buf);

private:
  static unsigned short rows, cols;

};

IMPLEMENTATION [ux]:

#include <cstdio>
#include <cstdlib>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "globals.h"
#include "initcalls.h"
#include "jdb_core.h"
#include "jdb_dbinfo.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "kernel_thread.h"
#include "keycodes.h"
#include "kmem.h"
#include "libc_support.h"
#include "logdefs.h"
#include "mem_layout.h"
#include "push_console.h"
#include "simpleio.h"
#include "space.h"
#include "static_init.h"
#include "thread.h"
#include "thread_state.h"
#include "trap_state.h"
#include "usermode.h"
#include "vkey.h"

int (*Jdb::bp_test_log_only)();
int (*Jdb::bp_test_break)(String_buffer *buf);

unsigned short Jdb::rows, Jdb::cols;

static Proc::Status jdb_irq_state;

IMPLEMENT inline
void
Jdb::enter_trap_handler(Cpu_number)
{
  conf_screen();

  // Set terminal raw mode
  enter_getchar();

  // Flush all output streams
  fflush (NULL);
}

IMPLEMENT inline
void
Jdb::leave_trap_handler(Cpu_number)
{
  // Restore terminal mode
  leave_getchar();

  // Flush all output streams
  fflush (NULL);

}

IMPLEMENT inline
bool
Jdb::handle_user_request(Cpu_number)
{ return false; }

IMPLEMENT inline
bool
Jdb::test_checksums()
{ return true; }


// disable interrupts before entering the kernel debugger
IMPLEMENT
void
Jdb::save_disable_irqs(Cpu_number cpu)
{
  assert(cpu == Cpu_number::boot_cpu());
  (void)cpu;
  jdb_irq_state = Proc::cli_save();
}

// restore interrupts after leaving the kernel debugger
IMPLEMENT
void
Jdb::restore_irqs(Cpu_number cpu)
{
  assert(cpu == Cpu_number::boot_cpu());
  (void)cpu;
  Proc::sti_restore(jdb_irq_state);
}

STATIC_INITIALIZE_P(Jdb,JDB_INIT_PRIO);

PUBLIC static FIASCO_INIT
void
Jdb::init()
{
  // Install JDB handler
  Trap_state::base_handler = (Trap_state::Handler)enter_jdb;

  // be sure that Push_console comes very first
  Kconsole::console()->register_console(push_cons()),

  register_libc_atexit(leave_getchar);
  atexit(leave_getchar);

  Thread::set_int3_handler(handle_int3_threadctx);
}

PRIVATE
static void
Jdb::conf_screen()
{
  struct winsize win;
  ioctl (fileno (stdin), TIOCGWINSZ, &win);
  rows = win.ws_row;
  cols = win.ws_col;

  if (rows < MIN_SCREEN_HEIGHT || cols < MIN_SCREEN_WIDTH)
    printf ("%sTerminal probably too small, should be at least %dx%d!\033[0m\n",
	    esc_emph, MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);

  Jdb_screen::set_height(rows);
  Jdb_screen::set_width(cols);
}

/** handle int3 debug extension */
PUBLIC static inline NOEXPORT
int
Jdb::int3_extension()
{
  Jdb_entry_frame *entry_frame = Jdb::entry_frame.cpu(Cpu_number::boot_cpu());
  Address      addr = entry_frame->ip();
  Address_type user = (entry_frame->cs() & 3) ? ADDR_USER : ADDR_KERNEL;
  Unsigned8    todo = peek ((Unsigned8 *) addr, user);
  Space *space = NULL; //get_task_id(0);
  error_buffer.cpu(Cpu_number::boot_cpu()).clear();

  if (todo == 0x3c && peek ((Unsigned8 *) (addr+1), user) == 13)
    {
      enter_getchar();
      entry_frame->_ax = Vkey::get();
      Vkey::clear();
      leave_getchar();
      return 1;
    }
  else if (todo != 0xeb)
    {
      error_buffer.cpu(Cpu_number::boot_cpu()).printf("INT 3");
      return 0;
    }

  // todo == 0xeb => enter_kdebug()
  Mword i;
  Mword len = peek ((Unsigned8 *) ++addr, user);

  if (len > 2 &&
      peek (((Unsigned8 *) addr + 1), user) == '*' &&
      peek (((Unsigned8 *) addr + 2), user) == '#')
    {
      char c = peek (((Unsigned8 *) addr + 3), user);

      if ((c == '#')
	  ? execute_command_ni(space, (char const *) entry_frame->_ax)
	  : execute_command_ni(space, (char const *)(addr + 3), len-2))
	return 1; // => leave Jdb
    }

  for (i = 0; i < len; i++)
    error_buffer.cpu(Cpu_number::boot_cpu()).append(peek ((Unsigned8 *) ++addr, user));
  error_buffer.cpu(Cpu_number::boot_cpu()).terminate();
  return 0;
}

static
bool
Jdb::handle_special_cmds(int)
{ return 1; }

IMPLEMENT
bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  error_buffer.cpu(cpu).clear();
  switch (entry_frame.cpu(cpu)->_trapno)
    {
      case 1:
        error_buffer.cpu(cpu).printf("Interception");
        break;
      case 3:
	if (int3_extension())
          return false;
#ifdef FIXME
        if (get_thread(cpu)->d_taskno())
	  {
	    if (bp_test_log_only && bp_test_log_only())
	      return false;
	    if (bp_test_break
		&& bp_test_break(&error_buffer.cpu(cpu)))
	      break;
	  }
#endif
    }

  return true;
}

IMPLEMENT
void
Jdb::handle_nested_trap(Jdb_entry_frame *e)
{
  printf("Trap in JDB: IP:%08lx\n", e->ip());
}

IMPLEMENT inline
bool
Jdb::handle_conditional_breakpoint(Cpu_number, Jdb_entry_frame *)
{ return false; }


PUBLIC
static Space *
Jdb::translate_task(Address /*addr*/, Space *task)
{
  // we have no idea if addr belongs to kernel or user space
  // since kernel and user occupy different address spaces
  return task;
}

PUBLIC
static Address
Jdb::virt_to_kvirt(Address virt, Mem_space* space)
{
  Mem_space::Phys_addr phys;
  Mem_space::Page_order size;

  if (!space)
    {
      // Kernel address.
      // We can directly access it via virtual addresses if it's kernel code
      // (which is always mapped, but doesn't appear in the kernel pagetable)
      //  or if we find a mapping for it in the kernel's master pagetable.
      return ((virt >= (Address)&Mem_layout::load &&
               virt <  (Kernel_thread::init_done()
                        ? (Address)&Mem_layout::end
                        : (Address)&Mem_layout::initcall_end))
              || Kernel_task::kernel_task()->virt_to_phys(virt) != ~0UL)
             ? virt
             : (Address) -1;
    }
  else
    {
      // User address.
      // We can't directly access it because it's in a different host process
      // but if the task's pagetable has a mapping for it, we can translate
      // task-virtual -> physical -> kernel-virtual address and then access.
      Virt_addr va(virt);
      return (space->v_lookup(va, &phys, &size, 0))
	? (Address) Kmem::phys_to_virt(Mem_space::Phys_addr::val(phys) + Virt_size::val(cxx::get_lsb(va, size)))
	: (Address) -1;
    }
}

IMPLEMENT inline NEEDS ["space.h"]
template <typename T>
T
Jdb::peek (T const *addr, Address_type user)
{
  // FIXME: assume UP here (current_meme_space(0))
  return Mem_space::current_mem_space(Cpu_number::boot_cpu())->peek(addr, user);
}

PUBLIC static
int
Jdb::peek_task(Address virt, Space *space, void *value, int width)
{
  // make sure we don't cross a page boundary
  if (virt & (width-1))
    return -1;

  Address kvirt = virt_to_kvirt(virt, space);
  if (kvirt == (Address)-1)
    return -1;

  memcpy(value, (void*)kvirt, width);
  return 0;
}

PUBLIC static
int
Jdb::poke_task(Address virt, Space *space, void const *value, int width)
{
  // make sure we don't cross a page boundary
  if (virt & (width-1))
    return -1;

  Address kvirt = virt_to_kvirt(virt, space);

  if (kvirt == (Address)-1)
    return -1;

  memcpy((void*)kvirt, value, width);
  return 0;
}

PUBLIC
static int
Jdb::is_adapter_memory(Address /*addr*/, Space * /*task*/)
{
  return 0;
}

#define WEAK __attribute__((weak))
extern "C" char in_slowtrap, in_page_fault, in_handle_fputrap;
extern "C" char in_interrupt, in_timer_interrupt, in_timer_interrupt_slow;
extern "C" char i30_ret_switch WEAK, in_slow_ipc1 WEAK;
extern "C" char in_slow_ipc2 WEAK, in_slow_ipc4;
extern "C" char in_sc_ipc1 WEAK, in_sc_ipc2 WEAK, in_syscall WEAK;
#undef WEAK

/** Try to guess the thread state of t by walking down the kernel stack and
 * locking at the first return address we find. */
PUBLIC
static Jdb::Guessed_thread_state
Jdb::guess_thread_state(Thread *t)
{
  Guessed_thread_state state = s_unknown;
  Mword *ktop = (Mword*)((Mword)context_of(t->get_kernel_sp()) +
			 Context::Size);

  for (int i=-1; i>-26; i--)
    {
      if (ktop[i] != 0)
	{
	  if (ktop[i] == (Mword)&in_page_fault)
	    state = s_pagefault;
	  if ((ktop[i] == (Mword)&i30_ret_switch) ||// shortcut.S, int 0x30
	      (ktop[i] == (Mword)&in_slow_ipc1) ||  // shortcut.S, int 0x30
	      (ktop[i] == (Mword)&in_slow_ipc4) ||  // entry.S, int 0x30
#if !defined(CONFIG_ASSEMBLER_IPC_SHORTCUT)
	      (ktop[i] == (Mword)&in_sc_ipc1)   ||  // entry.S, int 0x30
	      (ktop[i] == (Mword)&in_sc_ipc2)   ||  // entry.S, sysenter
#endif
	     0)
	    state = s_ipc;
	  else if (ktop[i] == (Mword)&in_syscall)
	    state = s_syscall;
	  else if (ktop[i] == (Mword)&Thread::user_invoke)
	    state = s_user_invoke;
	  else if (ktop[i] == (Mword)&in_handle_fputrap)
	    state = s_fputrap;
	  else if (ktop[i] == (Mword)&in_interrupt)
	    state = s_interrupt;
	  else if ((ktop[i] == (Mword)&in_timer_interrupt) ||
		   (ktop[i] == (Mword)&in_timer_interrupt_slow))
	    state = s_timer_interrupt;
	  else if (ktop[i] == (Mword)&in_slowtrap)
	    state = s_slowtrap;
	  if (state != s_unknown)
	    break;
	}
    }

  if (state == s_unknown && (t->state() & Thread_ipc_mask))
    state = s_ipc;

  return state;
}

// Don't make these members of Jdb else we have to include <termios.h>
// into the Jdb interface ...
static struct termios raw, new_raw;
static int    getchar_entered;

/** prepare Linux console for raw input */
PUBLIC static
void
Jdb::enter_getchar()
{
  if (!getchar_entered++)
    {
      tcgetattr (fileno (stdin), &raw);
      memcpy(&new_raw, &raw, sizeof(new_raw));
      new_raw.c_lflag    &= ~(ICANON|ECHO);
      new_raw.c_cc[VMIN]  = 0;
      new_raw.c_cc[VTIME] = 1;
      tcsetattr (fileno (stdin), TCSAFLUSH, &new_raw);
    }
}

/** restore Linux console. */
PUBLIC static
void
Jdb::leave_getchar()
{
  if (!--getchar_entered)
    tcsetattr (fileno (stdin), TCSAFLUSH, &raw);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [ux && mp]:

static
void
Jdb::send_nmi(Cpu_number)
{
}
