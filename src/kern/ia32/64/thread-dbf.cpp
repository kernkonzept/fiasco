INTERFACE[amd64]:

EXTENSION class Thread
{
private:
  static void handle_double_fault (Trap_state *) asm ("thread_handle_double_fault");

public:
  static bool may_enter_jdb;
};


IMPLEMENTATION[amd64]:

#include <cstdio>
#include "cpu.h"
#include "kernel_console.h"
#include "processor.h"
#include "reset.h"
#include "trap_state.h"
#include "tss.h"
#include "watchdog.h"


bool Thread::may_enter_jdb = false;

IMPLEMENT
void
Thread::handle_double_fault (Trap_state *ts)
{
  int c;

  Watchdog::disable();
     printf ("\n\033[1;31mDOUBLE FAULT!\033[m\n"
             "RAX=%016lx  RSI=%016lx\n"
	     "RBX=%016lx  RDI=%016lx\n"
	     "RCX=%016lx  RBP=%016lx\n"
	     "RDX=%016lx  RSP=%016lx\n"
	     "R8= %016lx  R9= %016lx\n"
	     "R10=%016lx  R11=%016lx\n"
	     "R12=%016lx  R13=%016lx\n"
	     "R14=%016lx  R15=%016lx\n"
	     "RIP %016lx  RFLAGS %016lx\n"
	     "CS %04lx    SS %04lx          CPU=%u\n\n",
	     ts->_ax, ts->_si,
	     ts->_bx, ts->_di,
	     ts->_cx, ts->_bp,
	     ts->_dx, ts->sp(),
	     ts->_r8,  ts->_r9,
	     ts->_r10, ts->_r11,
	     ts->_r12, ts->_r13,
	     ts->_r14, ts->_r15,
	     ts->ip(), ts->flags(),
	     ts->cs() & 0xffff, ts->ss() & 0xffff,
	     cxx::int_value<Cpu_number>(current_cpu()));

  if (may_enter_jdb)
    {
      puts ("Return reboots, \"k\" tries to enter the L4 kernel debugger...");

      while ((c=Kconsole::console()->getchar(false)) == -1)
	Proc::pause();

      if (c == 'k' || c == 'K')
	{
	     nested_trap_handler(ts,0); // XXX: 0 is possibly the wrong CPU
	}
    }
  else
    {
      puts ("Return reboots");
      while ((Kconsole::console()->getchar(false)) == -1)
	Proc::pause();
    }

  puts ("\033[1mRebooting...\033[0m");
  platform_reset();
}


