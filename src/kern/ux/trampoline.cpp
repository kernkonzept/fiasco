
INTERFACE:

#include <sys/types.h>			// for pid_t
#include "types.h"			// for Mword

class Trampoline
{};

IMPLEMENTATION:

#include <csignal>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include "undef_page.h"			// this undef's crap in <sys/user.h>
#include "mem_layout.h"

PRIVATE static inline NOEXPORT
void
Trampoline::wait_for_stop (pid_t pid)
{
  int status;

  do	// Loop until we get SIGTRAP. We might get orphaned SIGIO in between
    {
      ptrace (PTRACE_SYSCALL, pid, NULL, NULL);
      waitpid (pid, &status, 0);
    }
  while (WIFSTOPPED (status) && WSTOPSIG (status) != SIGTRAP);
}

/*
 * Trampoline Operations
 *
 * These run on the trampoline page which is shared between all tasks and
 * mapped into their respective address space. phys_magic is the bottom of
 * the magic page where the magic code runs, phys_magic + pagesize is the
 * top of the magic page, which the code uses as stack.
 */
PUBLIC static
void
Trampoline::syscall (pid_t pid, Mword eax = 0, Mword ebx = 0,
                                Mword ecx = 0, Mword edx = 0)
{
  struct user_regs_struct regs, tramp_regs;

  // don't perform syscalls without PID -- should only happen in tests
  if (!pid)
    return;

  ptrace (PTRACE_GETREGS, pid, NULL, &regs);		// Save registers

  tramp_regs     = regs;				// Copy registers
  tramp_regs.eax = eax;
  tramp_regs.ebx = ebx;
  tramp_regs.ecx = ecx;
  tramp_regs.edx = edx;
  tramp_regs.eip = Mem_layout::Trampoline_page;

  *(Mword *) Mem_layout::kernel_trampoline_page = 0x80cd;

  ptrace (PTRACE_SETREGS, pid, NULL, &tramp_regs);	// Setup trampoline

  wait_for_stop (pid);					// Kernel entry
  wait_for_stop (pid);					// Kernel exit

  ptrace (PTRACE_SETREGS, pid, NULL, &regs);		// Restore registers
}
