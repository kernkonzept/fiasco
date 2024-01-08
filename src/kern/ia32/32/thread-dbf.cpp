INTERFACE:

EXTENSION class Thread
{
private:
  static void handle_double_fault (void) asm ("thread_handle_double_fault");

public:
  static bool may_enter_jdb;
};

IMPLEMENTATION:

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
Thread::handle_double_fault (void)
{
  // cannot use current_cpu() here because this must run on a thread stack,
  // not on a dbf stack
  volatile Tss *tss = Cpu::boot_cpu()->get_tss();

  Watchdog::disable();

  printf("\n"
         "\033[1;31mDOUBLE FAULT!\033[m\n"
         "EAX=%08x  ESI=%08x  DS=%04x\n"
         "EBX=%08x  EDI=%08x  ES=%04x\n"
         "ECX=%08x  EBP=%08x  GS=%04x\n"
         "EDX=%08x  ESP=%08x  SS=%04x   ESP0=%08lx\n"
         "EFL=%08x  EIP=%08x  CS=%04x    CPU=%u\n",
         tss->_hw.ctx.eax,    tss->_hw.ctx.esi, tss->_hw.ctx.ds & 0xffff,
         tss->_hw.ctx.ebx,    tss->_hw.ctx.edi, tss->_hw.ctx.es & 0xffff,
         tss->_hw.ctx.ecx,    tss->_hw.ctx.ebp, tss->_hw.ctx.gs & 0xffff,
         tss->_hw.ctx.edx,    tss->_hw.ctx.esp, tss->_hw.ctx.ss & 0xffff,
         tss->_hw.ctx.esp0,
         tss->_hw.ctx.eflags, tss->_hw.ctx.eip, tss->_hw.ctx.cs & 0xffff,
         cxx::int_value<Cpu_number>(current_cpu()));

  if (may_enter_jdb)
    {
      puts("Return reboots, \"k\" tries to enter the L4 kernel debugger...");

      int c;

      while ((c = Kconsole::console()->getchar(false)) == -1)
        Proc::pause();

      if (c == 'k' || c == 'K')
        {
          Mword dummy;
          Trap_state ts;

          // built a nice trap state the jdb can work with
          ts._ax    = tss->_hw.ctx.eax;
          ts._bx    = tss->_hw.ctx.ebx;
          ts._cx    = tss->_hw.ctx.ecx;
          ts._dx    = tss->_hw.ctx.edx;
          ts._si    = tss->_hw.ctx.esi;
          ts._di    = tss->_hw.ctx.edi;
          ts._bp    = tss->_hw.ctx.ebp;
          ts.sp(tss->_hw.ctx.esp);
          ts.cs(tss->_hw.ctx.cs);
          ts._ds     = tss->_hw.ctx.ds;
          ts._es     = tss->_hw.ctx.es;
          ts.ss(tss->_hw.ctx.ss);
          ts._fs     = tss->_hw.ctx.fs;
          ts._gs     = tss->_hw.ctx.gs;
          ts._trapno = 8;
          ts._err    = 0;
          ts.ip(tss->_hw.ctx.eip);
          ts.flags(tss->_hw.ctx.eflags);

          asm volatile (
            "call *%2\n\t"
             : "=a" (dummy)
             : "a" (&ts), "m" (nested_trap_handler)
             : "ecx", "edx", "memory"
          );
        }
    }
  else
    {
      puts("Return reboots");
      while ((Kconsole::console()->getchar(false)) == -1)
        Proc::pause();
    }

  puts("\033[1mRebooting...\033[0m");
  platform_reset();
}
