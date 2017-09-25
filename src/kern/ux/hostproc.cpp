
/*
 * Fiasco-UX
 * Functions for setting up host processes implementing tasks
 */

INTERFACE:

class Hostproc
{};

IMPLEMENTATION:

#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cassert>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "boot_info.h"
#include "config.h"
#include "cpu_lock.h"
#include "globals.h"
#include "lock_guard.h"
#include "mem_layout.h"
#include "panic.h"
#include "trampoline.h"

PRIVATE static
void
Hostproc::setup()
{
  char **args = Boot_info::args();
  size_t arglen = 0;

  // Zero all args, making ps output pretty
  do arglen += strlen (*args) + 1; while (*++args);
  memset (*Boot_info::args(), 0, arglen);

  // Put task number into argv[0] for ps
  snprintf (*Boot_info::args(), arglen, "[Task]");

  fclose (stdin);
  fclose (stdout);
  fclose (stderr);

  sigset_t mask;
  sigfillset (&mask);
  check (!sigprocmask (SIG_UNBLOCK, &mask, NULL));

  stack_t stack;
  stack.ss_sp    = (void *) Mem_layout::Trampoline_page;
  stack.ss_size  = Config::PAGE_SIZE;
  stack.ss_flags = 0;
  check (!sigaltstack (&stack, NULL));

  struct sigaction action;
  sigfillset (&action.sa_mask);
  action.sa_flags     = SA_ONSTACK | SA_SIGINFO;
  action.sa_restorer  = (void (*)()) 0xDEADC0DE;
  action.sa_sigaction = (void (*)(int,siginfo_t*,void*)) 
			    Mem_layout::Trampoline_page;
  check (!sigaction (SIGSEGV, &action, NULL));

  // Map trampoline page
  check (mmap ((void *) Mem_layout::Trampoline_page,
         Config::PAGE_SIZE,
	     PROT_READ | PROT_WRITE | PROT_EXEC,
	     MAP_SHARED | MAP_FIXED,
         Boot_info::fd(),
         Mem_layout::Trampoline_frame) != MAP_FAILED);

  ptrace (PTRACE_TRACEME, 0, NULL, NULL);

  raise (SIGUSR1);
}

PUBLIC static
unsigned
Hostproc::create()
{
  auto guard = lock_guard(cpu_lock);

  static unsigned long esp;
  static Mword _stack[256];
  pid_t pid;

  /*
   * Careful with local variables here because we are changing the
   * stack pointer for the fork system call. This ensures that the
   * child gets its own COW stack rather than running on the parent
   * stack.
   */
  asm volatile("movl %%esp, %0 \n\t"
               "movl %2, %%esp \n\t"
	       "call _ZN8Hostproc7do_forkEv\n\t"
	       "movl %0, %%esp \n\t"
               : "=m" (esp), "=a" (pid)
               : "r" (&_stack[sizeof(_stack) / sizeof(_stack[0])]));

  switch (pid)
  {
      case -1:                            // Failed
        return 0;

      default:                            // Parent
        int status;
        check (waitpid (pid, &status, 0) == pid);
        assert (WIFSTOPPED (status) && WSTOPSIG (status) == SIGUSR1);

        Trampoline::syscall (pid, __NR_munmap, 0, Mem_layout::Trampoline_page);
        return pid;
  }
}

PRIVATE static
int
Hostproc::do_fork()
{
  // Make sure the child doesn't inherit any of our unflushed output
  fflush (NULL);

  switch (pid_t pid = fork())
    {
      case 0:                             // Child
        setup();
        _exit(1);                         // unreached

      default:
	return pid;
    }
}
