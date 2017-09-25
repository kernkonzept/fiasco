
INTERFACE:

#include <csignal>				// for siginfo_t
#include "initcalls.h"
#include "types.h"

class Usermode
{};

IMPLEMENTATION:

#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <panic.h>
#include <ucontext.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include "undef_page.h"

#include "boot_info.h"
#include "config.h"
#include "config_tcbsize.h"
#include "context.h"
#include "emulation.h"
#include "fpu.h"
#include "globals.h"
#include "mem_layout.h"
#include "irq_chip_ux.h"
#include "processor.h"
#include "regdefs.h"
#include "task.h"
#include "thread.h"
#include "thread_state.h"

PRIVATE static
Mword
Usermode::peek_at_addr (pid_t pid, Address addr, unsigned n)
{
  Mword val;

  if ((addr & (sizeof (Mword) - 1)) + n > sizeof (Mword))
    val = ptrace (PTRACE_PEEKTEXT, pid, addr, NULL);
  else
    val = ptrace (PTRACE_PEEKTEXT, pid, addr & ~(sizeof (Mword) - 1), NULL) >>
          CHAR_BIT * (addr & (sizeof (Mword) - 1));

  return val & (Mword) -1 >> CHAR_BIT * (sizeof (Mword) - n);
}

/**
 * Wait for host process to stop.
 * @param pid process id to wait for.
 * @return signal the host process stopped with.
 */
PRIVATE static inline NOEXPORT
int
Usermode::wait_for_stop (pid_t pid)
{
  int status;

  check (waitpid (pid, &status, 0) == pid && WIFSTOPPED (status));

  return WSTOPSIG (status);
}

/**
 * Set emulated internal processor interrupt state.
 * @param mask signal mask to modify
 * @param eflags processor flags register
 */
PRIVATE static inline NOEXPORT
void
Usermode::sync_interrupt_state (sigset_t *mask, Mword eflags)
{
  Proc::ux_set_virtual_processor_state (eflags);

  if (!mask)
    return;

  if (eflags & EFLAGS_IF)
    sigdelset (mask, SIGIO);
  else
    sigaddset (mask, SIGIO);
}

/**
 * Cancel a native system call in the host.
 * @param pid process id of the host process.
 * @param regs register set at the time of the system call.
 */
PRIVATE static inline NOEXPORT
void
Usermode::cancel_syscall (pid_t pid, struct user_regs_struct *regs)
{
  ptrace (PTRACE_POKEUSER, pid, offsetof (struct user, regs.orig_eax), -1);
  ptrace (PTRACE_SYSCALL, pid, NULL, NULL);

  wait_for_stop (pid);

  regs->eax = regs->orig_eax;
  ptrace (PTRACE_POKEUSER, pid, offsetof (struct user, regs.eax), regs->eax);
}

/**
 * Read debug register
 * @param pid process id of the host process.
 * @param reg number of debug register (0..7)
 * @param value reference to register value
 * @return 0 is ok
 */
PUBLIC static
int
Usermode::read_debug_register (pid_t pid, Mword reg, Mword &value)
{
  if (reg > 7)
    return 0;

  int ret = ptrace (PTRACE_PEEKUSER, pid,
                   ((struct user *) 0)->u_debugreg + reg, NULL);

  if (ret == -1 && errno == -1)
    return 0;

  value = ret;

  return 1;
}

/**
 * Write debug register
 * @param pid process id of the host process.
 * @param reg number of debug register (0..7)
 * @param value register value to be written.
 * @return 0 is ok
 */
PUBLIC static
int
Usermode::write_debug_register (pid_t pid, Mword reg, Mword value)
{
  if (reg > 7)
    return 0;

  if (ptrace (PTRACE_POKEUSER, pid,
             ((struct user *) 0)->u_debugreg + reg, value) == -1)
    return 0;

  return 1;
}

/**
 * Set up kernel stack for kernel entry through an interrupt gate.
 * We are running on the signal stack and modifying the interrupted context
 * on the signal stack to allow us to return anywhere.
 * Depending on the value of the code segment (CS) we set up a processor
 * context of 3 (kernel mode) or 5 (user mode) words on the respective kernel
 * stack, which is the current stack (kernel mode) or the stack determined by
 * the Task State Segment (user mode). For some traps we need to put an
 * additional error code on the stack.
 * This is precisely what an ia32 processor does in hardware.
 * @param context Interrupted context on signal stack
 * @param trap Trap number that caused kernel entry (0xffffffff == shutdown)
 * @param xss Stack Segment
 * @param esp Stack Pointer
 * @param efl EFLAGS
 * @param xcs Code Segment
 * @param eip Instruction Pointer
 * @param err Error Code
 * @param cr2 Page Fault Address (if applicable)
 */
PRIVATE static
void
Usermode::kernel_entry(Cpu_number _cpu,
                       struct ucontext *context,
                       Mword trap,
                       Mword xss,
                       Mword esp,
                       Mword efl,
                       Mword xcs,
                       Mword eip,
                       Mword err,
                       Mword cr2)
{
  Mword *kesp = (xcs & 3) == 3
              ? (Mword *) Cpu::cpus.cpu(_cpu).kernel_sp() - 5
              : (Mword *) context->uc_mcontext.gregs[REG_ESP] - 3;

  if (!Thread::is_tcb_address((Address)kesp))
    {
      printf("KERNEL BUG at EIP:%08x ESP:%08x -- PFA:%08lx kesp=%p trap=%lx xcs=%lx @ %p %lx\n",
             context->uc_mcontext.gregs[REG_EIP],
             context->uc_mcontext.gregs[REG_ESP],
             context->uc_mcontext.cr2, kesp, trap, xcs, &Cpu::cpus.cpu(_cpu).kernel_sp(), Cpu::cpus.cpu(_cpu).kernel_sp());
      abort();
    }

  // Make sure the kernel stack is sane
  assert (Thread::is_tcb_address((Address)kesp));

  // Make sure the kernel stack has enough space
  if ((Mword) kesp % THREAD_BLOCK_SIZE <= 512)
    {
      printf("KERNEL BUG: Kernel stack of thread ");
      printf("DBGID=%lx\n", static_cast<Thread*>(context_of(kesp))->dbg_info()->dbg_id());
      panic(" exceeded (%p, %c). \n"
            "            As a workaround, please make sure that you built \n"
            "            Fiasco-UX with enabled CONTEXT_4K.",
            kesp, (xcs & 3) == 2 ? 'k' : 'u');
    }

  efl &= ~EFLAGS_IF;

  switch (xcs & 3)
    {
      case 3:
        *(kesp + 4) = xss;
        *(kesp + 3) = esp;

      case 0:
        *(kesp + 2) = efl | (Proc::processor_state() & EFLAGS_IF);
        *(kesp + 1) = xcs & ~1; // trap on iret
        *(kesp + 0) = eip;
    }

  switch (trap)
    {
      case 0xe:					// Page Fault
        Emulation::set_page_fault_addr (cr2);
      case 0x8:					// Double Fault
      case 0xa:					// Invalid TSS
      case 0xb:					// Segment Not Present
      case 0xc:					// Stack Fault
      case 0xd:					// General Protection Fault
      case 0x11:				// Alignment Check
        *--kesp = err;
    }

  context->uc_mcontext.gregs[REG_ESP] = (Mword) kesp;
  context->uc_mcontext.gregs[REG_EIP] = Emulation::idt_vector (trap, false);
  context->uc_mcontext.gregs[REG_EFL] = efl & ~(EFLAGS_TF | EFLAGS_NT | EFLAGS_RF | EFLAGS_VM);
  sync_interrupt_state (&context->uc_sigmask, efl);

  // Make sure interrupts are off
  assert (!Proc::interrupts());
}

PRIVATE static inline NOEXPORT
Mword
Usermode::kip_syscall (Address eip)
{
  if ((eip & Config::PAGE_MASK) != Mem_layout::Syscalls || eip & 0xff)
    return 0;

  Mword trap = 0x30 + ((eip - Mem_layout::Syscalls) >> 8);

  return Emulation::idt_vector (trap, true) ? trap : 0;
}

PRIVATE static inline NOEXPORT
Mword
Usermode::l4_syscall (Mword opcode)
{
  if (EXPECT_FALSE ((opcode & 0xff) != 0xcd))
    return 0;

  Mword trap = opcode >> 8;

  return Emulation::idt_vector (trap, true) ? trap : 0;
}

PRIVATE static inline NOEXPORT NEEDS["thread_state.h"]
bool
Usermode::user_exception(Cpu_number _cpu, pid_t pid, struct ucontext *context,
                         struct user_regs_struct *regs)
{
  Mword trap, error = 0, addr = 0;

  if (EXPECT_FALSE ((trap = kip_syscall (regs->eip))))
    {
      Context *t = context_of(((Mword *)Cpu::cpus.cpu(_cpu).kernel_sp()) - 1);

      /* The alien syscall code in entry-*.S substracts 2 bytes from the
       * EIP to put the EIP back on the instruction to reexecute it.
       * 'int X' and sysenter etc. are 2 byte instructions.
       * So we add 2 here to have the EIP in the right position afterwards.
       *
       * Furthermore we leave ESP and EIP (with the adjustment) where they
       * are so that the syscall can be re-executed.
       *
       * This is not a problem for native as it does not trap on the
       * 'call 0xea......' itself there but on the real int/sysenter/etc.
       * instructions in the syscall page.
       */
      if (EXPECT_FALSE((t->state() & (Thread_alien | Thread_dis_alien))
                       == Thread_alien || t->space_ref()->user_mode()))
        regs->eip += 2;
      else
        {
          regs->eip  = peek_at_addr (pid, regs->esp, 4);
          regs->esp += 4;
        }
    }

  else if ((trap = l4_syscall (peek_at_addr (pid, regs->eip, 2))))
    regs->eip += 2;

  else
    {
      struct ucontext *exception_context;

      memcpy ((void *) Mem_layout::kernel_trampoline_page,
              (void *) &Mem_layout::task_sighandler_start,
              &Mem_layout::task_sighandler_end -
              &Mem_layout::task_sighandler_start);

      ptrace (PTRACE_CONT, pid, NULL, SIGSEGV);

      wait_for_stop (pid);

      // See corresponding code in sighandler.S
      exception_context = reinterpret_cast<struct ucontext *>
                          (Mem_layout::kernel_trampoline_page +
                         *reinterpret_cast<Address *>
                          (Mem_layout::kernel_trampoline_page + 0x100));

      addr  = exception_context->uc_mcontext.cr2;
      trap  = exception_context->uc_mcontext.gregs[REG_TRAPNO];
      error = exception_context->uc_mcontext.gregs[REG_ERR];

      switch (trap)
        {
          case 0xd:
	    if (Boot_info::emulate_clisti())
	      switch (peek_at_addr (pid, regs->eip, 1))
		{
		  case 0xfa:	// cli
		    Irq_chip_ux::set_owner(Boot_info::pid());
		    regs->eip++;
		    regs->eflags &= ~EFLAGS_IF;
		    sync_interrupt_state (0, regs->eflags);
		    check(ptrace (PTRACE_SETREGS, pid, NULL, regs));
		    return false;

		  case 0xfb:	// sti
		    Irq_chip_ux::set_owner(pid);
		    regs->eip++;
		    regs->eflags |= EFLAGS_IF;
		    sync_interrupt_state (0, regs->eflags);
		    check(ptrace (PTRACE_SETREGS, pid, NULL, regs));
		    return false;
		}
            break;

          case 0xe:
            error |= PF_ERR_USERADDR;
            break;
        }
    }

  kernel_entry (_cpu, context, trap,
                regs->xss,      /* XSS */
                regs->esp,	/* ESP */
                regs->eflags,   /* EFL */
                regs->xcs,      /* XCS */
                regs->eip,      /* EIP */
                error,          /* ERR */
                addr);          /* CR2 */

  return true;
}

PRIVATE static inline NOEXPORT
bool
Usermode::user_emulation(Cpu_number _cpu, int stop, pid_t pid,
                         struct ucontext *context,
                         struct user_regs_struct *regs)
{
  Mword trap, error = 0;

  switch (stop)
    {
      case SIGSEGV:
        return user_exception (_cpu, pid, context, regs);

      case SIGIO:
        trap = Irq_chip_ux::pending_vector();
        if ((int)trap == -1)
          return false;
        break;

      case SIGTRAP:
        if (peek_at_addr (pid, regs->eip - 1, 1) == 0xcc)
          {
            trap = 0x3;
            break;
          }
        else if (peek_at_addr (pid, regs->eip - 2, 2) == 0x80cd)
          {
            cancel_syscall (pid, regs);
            trap       = 0xd;
            error      = 0x80 << 3 | 2;
            regs->eip -= 2;
            break;
          }
        trap = 0x1;
        break;

      case SIGILL:
        trap = 0x6;
        break;

      case SIGFPE:
        trap = 0x10;
        break;

      default:
        trap = 0x1;
        break;
    }

  kernel_entry (_cpu, context, trap,
                regs->xss,      /* XSS */
                regs->esp,	/* ESP */
                regs->eflags,   /* EFL */
                regs->xcs,      /* XCS */
                regs->eip,      /* EIP */
                error,          /* ERR */
                0);             /* CR2 */

  return true;
}

/**
 * IRET to a user context.
 * We restore the saved context on the stack, namely EIP, CS, EFLAGS, ESP, SS.
 * Additionally all register values are transferred to the task's register set.
 * @param ctx Kern context during iret
 */
PRIVATE static inline NOEXPORT
void
Usermode::iret_to_user_mode(Cpu_number _cpu,
                            struct ucontext *context, Mword *kesp)
{
  struct user_regs_struct regs;
  Context *t = context_of (kesp);
  pid_t pid = t->vcpu_aware_space()->pid();

  Irq_chip_ux::set_owner(pid);

  /*
   * If there are any interrupts pending up to this point, don't start the task
   * but let it enter kernel immediately. Any interrupts occuring beyond this
   * point will go directly to the task.
   */
  int irq_pend = Irq_chip_ux::pending_vector();
  if (irq_pend != -1)
    {
      Irq_chip_ux::set_owner(Boot_info::pid());

      kernel_entry (_cpu, context, irq_pend,
                    *(kesp + 4),    /* XSS */
                    *(kesp + 3),    /* ESP */
                    *(kesp + 2),    /* EFL */
                    *(kesp + 1) | 3,/* XCS */
                    *(kesp + 0),    /* EIP */
                    0,              /* ERR */
                    0);             /* CR2 */
      return;
    }

  // Restore these from the kernel stack (iret context)
  regs.eip    = *(kesp + 0);
  regs.xcs    = *(kesp + 1) | 3;
  regs.eflags = *(kesp + 2);
  regs.esp    = *(kesp + 3);
  regs.xss    = *(kesp + 4);

  // Copy these from the kernel
  regs.eax    = context->uc_mcontext.gregs[REG_EAX];
  regs.ebx    = context->uc_mcontext.gregs[REG_EBX];
  regs.ecx    = context->uc_mcontext.gregs[REG_ECX];
  regs.edx    = context->uc_mcontext.gregs[REG_EDX];
  regs.esi    = context->uc_mcontext.gregs[REG_ESI];
  regs.edi    = context->uc_mcontext.gregs[REG_EDI];
  regs.ebp    = context->uc_mcontext.gregs[REG_EBP];
  regs.xds    = context->uc_mcontext.gregs[REG_DS];
  regs.xes    = context->uc_mcontext.gregs[REG_ES];
  regs.xfs    = Cpu::get_fs();
  regs.xgs    = Cpu::get_gs();

  // ptrace will return with an error if we try to load invalid values to
  // segment registers
  int r = ptrace (PTRACE_SETREGS, pid, NULL, &regs);
  if (EXPECT_FALSE(r == -EPERM))
    {
      WARN("Failure setting registers, probably invalid segment values.\n"
           "        Fixing up!\n");
      regs.xds = Cpu::kern_ds();
      regs.xes = Cpu::kern_es();
      regs.xgs = 0;
      check(ptrace (PTRACE_SETREGS, pid, NULL, &regs));
    }
  else
    assert(r == 0);

  Fpu::restore_state (t->fpu_state());

  for (;;)
    {
      ptrace (t->is_native() ? PTRACE_CONT :  PTRACE_SYSCALL, pid, NULL, NULL);

      int stop = wait_for_stop (pid);

      if (EXPECT_FALSE (stop == SIGWINCH || stop == SIGTERM || stop == SIGINT))
        continue;

      check(ptrace (PTRACE_GETREGS, pid, NULL, &regs) == 0);

      if (EXPECT_TRUE (user_emulation (_cpu, stop, pid, context, &regs)))
        break;
    }

  Irq_chip_ux::set_owner(Boot_info::pid());

  if (Irq_chip_ux::pending_vector() != -1)
    kill(Boot_info::pid(), SIGIO);

  context->uc_mcontext.gregs[REG_EAX] = regs.eax;
  context->uc_mcontext.gregs[REG_EBX] = regs.ebx;
  context->uc_mcontext.gregs[REG_ECX] = regs.ecx;
  context->uc_mcontext.gregs[REG_EDX] = regs.edx;
  context->uc_mcontext.gregs[REG_ESI] = regs.esi;
  context->uc_mcontext.gregs[REG_EDI] = regs.edi;
  context->uc_mcontext.gregs[REG_EBP] = regs.ebp;
  context->uc_mcontext.gregs[REG_DS]  = regs.xds;
  context->uc_mcontext.gregs[REG_ES]  = regs.xes;
  Cpu::set_fs(regs.xfs);
  Cpu::set_gs(regs.xgs);

  Fpu::save_state (t->fpu_state());
}

/**
 * IRET to a kernel context.
 * We restore the saved context on the stack, namely EIP and EFLAGS.
 * We do NOT restore CS, because the kernel thinks it has privilege level 0
 * but in usermode it has to have privilege level 3. We also adjust ESP by
 * 3 words, thus clearing the context from the stack.
 * @param ctx Kern context during iret
 */
PRIVATE static inline NOEXPORT
void
Usermode::iret_to_kern_mode (struct ucontext *context, Mword *kesp)
{
  context->uc_mcontext.gregs[REG_EIP]  = *(kesp + 0);
  context->uc_mcontext.gregs[REG_EFL]  = *(kesp + 2);
  context->uc_mcontext.gregs[REG_ESP] += 3 * sizeof (Mword);
}

/**
 * Emulate IRET instruction.
 * Depending on the value of the saved code segment (CS) on the kernel stack
 * we return to kernel mode (CPL == 0) or user mode (CPL == 2).
 * @param ctx Kern context during iret
 */
PRIVATE static inline NOEXPORT
void
Usermode::iret(Cpu_number _cpu, struct ucontext *context)
{
  Mword *kesp = (Mword *) context->uc_mcontext.gregs[REG_ESP];

  sync_interrupt_state (&context->uc_sigmask, *(kesp + 2));

  switch (*(kesp + 1) & 3)
    {
      case 0:			/* CPL 0 -> Kernel */
        iret_to_kern_mode (context, kesp);
        break;

      case 2:			/* CPL 2 -> User */
        iret_to_user_mode (_cpu, context, kesp);
        break;

      default:
	assert(0);
    }
}

PRIVATE static
void
Usermode::emu_handler (int, siginfo_t *, void *ctx)
{
  struct ucontext *context = reinterpret_cast<struct ucontext *>(ctx);
  unsigned int trap = context->uc_mcontext.gregs[REG_TRAPNO];

  Cpu_number _cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(Cpu::phys_id_direct()));

  if (trap == 0xd)	/* General protection fault */
    {
      unsigned char opcode = *reinterpret_cast<unsigned char *>
                             (context->uc_mcontext.gregs[REG_EIP]);

      switch (opcode)
        {
          case 0xfa:					/* cli */
            context->uc_mcontext.gregs[REG_EIP]++;
            context->uc_mcontext.gregs[REG_EFL] &= ~EFLAGS_IF;
            sync_interrupt_state (&context->uc_sigmask,
                                   context->uc_mcontext.gregs[REG_EFL]);
            return;

          case 0xfb:					/* sti */
            context->uc_mcontext.gregs[REG_EIP]++;
            context->uc_mcontext.gregs[REG_EFL] |= EFLAGS_IF;
            sync_interrupt_state (&context->uc_sigmask,
                                   context->uc_mcontext.gregs[REG_EFL]);
            return;

          case 0xcf:					/* iret */
            iret (_cpu, context);
            return;
        }
    }

  kernel_entry (_cpu, context, trap,
                context->uc_mcontext.gregs[REG_SS],
                context->uc_mcontext.gregs[REG_ESP],
                context->uc_mcontext.gregs[REG_EFL],
                context->uc_mcontext.gregs[REG_CS] & ~3,
                context->uc_mcontext.gregs[REG_EIP],
                context->uc_mcontext.gregs[REG_ERR] & ~PF_ERR_USERMODE,
                context->uc_mcontext.cr2);
}

PRIVATE static
void
Usermode::int_handler (int, siginfo_t *, void *ctx)
{
  struct ucontext *context = reinterpret_cast<struct ucontext *>(ctx);

  int gate = Irq_chip_ux::pending_vector();
  if (gate == -1)
    return;

  kernel_entry (Cpu::cpus.find_cpu(Cpu::By_phys_id(Cpu::phys_id_direct())),
                context,
                gate,
                context->uc_mcontext.gregs[REG_SS],     /* XSS */
                context->uc_mcontext.gregs[REG_ESP],    /* ESP */
                context->uc_mcontext.gregs[REG_EFL],    /* EFL */
                context->uc_mcontext.gregs[REG_CS] & ~3,/* XCS */
                context->uc_mcontext.gregs[REG_EIP],    /* EIP */
                0,                                      /* ERR */
                0);                                     /* CR2 */
}

PRIVATE static
void
Usermode::jdb_handler (int sig, siginfo_t *, void *ctx)
{
  struct ucontext *context = reinterpret_cast<struct ucontext *>(ctx);

  if (!Thread::is_tcb_address(context->uc_mcontext.gregs[REG_ESP]))
    return;

  /*
   * If a SIGSEGV is pending at the same time as SIGINT, i.e. because
   * someone pressed Ctrl-C on an sti instruction, SIGINT will be delivered
   * first. Since we warp to a different execution path the pending SIGSEGV
   * will then hit an innocent instruction elsewhere with fatal consequences.
   * Therefore a pending SIGSEGV must be cancelled - it will later reoccur.
   */

  signal (SIGSEGV, SIG_IGN);    // Cancel signal
  set_signal (SIGSEGV);         // Reinstall handler

  kernel_entry (Cpu::cpus.find_cpu(Cpu::By_phys_id(Cpu::phys_id_direct())),
                context, sig == SIGTRAP ? 3 : 1,
                context->uc_mcontext.gregs[REG_SS],     /* XSS */
                context->uc_mcontext.gregs[REG_ESP],	/* ESP */
                context->uc_mcontext.gregs[REG_EFL],	/* EFL */
                context->uc_mcontext.gregs[REG_CS] & ~3,/* XCS */
                context->uc_mcontext.gregs[REG_EIP],	/* EIP */
                0,                                      /* ERR */
                0);                                     /* CR2 */
}

PUBLIC static
void
Usermode::set_signal (int sig)
{
  void (*func)(int, siginfo_t *, void *);
  struct sigaction action;

  switch (sig)
    {
      case SIGIO:	func = int_handler;	break;
      case SIGSEGV:	func = emu_handler;	break;
      default:		func = jdb_handler;	break;
    }

  sigfillset (&action.sa_mask);		/* No other signals while we run */
  action.sa_sigaction = func;
  action.sa_flags     = SA_RESTART | SA_ONSTACK | SA_SIGINFO;

  check (sigaction (sig, &action, NULL) == 0);
}

PUBLIC static FIASCO_INIT_CPU
void
Usermode::init(Cpu_number cpu)
{
  stack_t stack;

  /* We want signals, aka interrupts to be delivered on an alternate stack */
  if (cpu == Cpu_number::boot_cpu())
    stack.ss_sp  = (void *) Mem_layout::phys_to_pmem
                                (Mem_layout::Sigstack_cpu0_start_frame);
  else
    stack.ss_sp = Kmem_alloc::allocator()->alloc(Mem_layout::Sigstack_log2_size);
  stack.ss_size  =  Mem_layout::Sigstack_size;
  stack.ss_flags = 0;

  check (sigaltstack (&stack, NULL) == 0);

  signal (SIGWINCH, SIG_IGN);
  signal (SIGPROF,  SIG_IGN);
  signal (SIGHUP,   SIG_IGN);
  signal (SIGUSR1,  SIG_IGN);
  signal (SIGUSR2,  SIG_IGN);

  set_signal (SIGSEGV);
  set_signal (SIGIO);
  if (cpu == Cpu_number::boot_cpu())
    set_signal (SIGINT);
  else
    signal (SIGINT, SIG_IGN);
  set_signal (SIGTRAP);
  set_signal (SIGTERM);
  set_signal (SIGXCPU);
}
