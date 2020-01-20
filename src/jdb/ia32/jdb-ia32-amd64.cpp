// Inside Jdb the method Jdb::get_thread() should be used instead of
// Thread::current_thread(). The latter function cannot not handle the
// case when we came from the kernel stack context!

INTERFACE:

#include "l4_types.h"
#include "pic.h"

class Trap_state;
class Thread;
class Console_buffer;
class Jdb_entry_frame;
class Push_console;

EXTENSION class Jdb
{
public:
  enum
  {
    Msr_test_default     = 0,
    Msr_test_fail_warn   = 1,
    Msr_test_fail_ignore = 2,
  };

  static void init();

  static Per_cpu<unsigned> apic_tpr;
  static Unsigned16 pic_status;
  static volatile char msr_test;
  static volatile char msr_fail;

  typedef enum
    {
      s_unknown, s_ipc, s_syscall, s_pagefault, s_fputrap,
      s_interrupt, s_timer_interrupt, s_slowtrap, s_user_invoke,
    } Guessed_thread_state;


  static int (*bp_test_log_only)(Cpu_number cpu, Jdb_entry_frame *ef);
  static int (*bp_test_sstep)(Cpu_number cpu, Jdb_entry_frame *ef);
  static int (*bp_test_break)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
  static int (*bp_test_other)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);

private:

  static char _connected;
  static Per_cpu<char> permanent_single_step;
  static Per_cpu<char> code_ret, code_call, code_bra, code_int;

  typedef enum
    {
      SS_NONE=0, SS_BRANCH, SS_RETURN
    } Step_state;

  static Per_cpu<Step_state> ss_state;
  static Per_cpu<int> ss_level;

  static const Unsigned8 *debug_ctrl_str;
  static int              debug_ctrl_len;

  static Per_cpu<int> jdb_irqs_disabled;
};

IMPLEMENTATION [{amd64,ia32}-!serial]:

static inline
void Jdb::init_serial_console()
{}

IMPLEMENTATION [{amd64,ia32}-serial]:

#include <cstdio>
#include "kernel_uart.h"

static
void Jdb::init_serial_console()
{
  if (Config::serial_esc == Config::SERIAL_ESC_IRQ &&
      !Kernel_uart::uart()->failed())
    {
      int irq;

      if ((irq = Kernel_uart::uart()->irq()) == -1)
	{
	  Config::serial_esc = Config::SERIAL_ESC_NOIRQ;
	  puts("SERIAL ESC: Using serial hack in slow timer handler.");
	}
      else
	{
	  Kernel_uart::enable_rcv_irq();
	  printf("SERIAL ESC: allocated IRQ %d for serial uart\n", irq);
	}
    }
}

IMPLEMENTATION[ia32,amd64]:

#include <cstring>
#include <csetjmp>
#include <cstdarg>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include "simpleio.h"

#include "apic.h"
#include "boot_info.h"
#include "checksum.h"
#include "config.h"
#include "cpu.h"
#include "initcalls.h"
#include "idt.h"
#include "io_apic.h"
#include "jdb_core.h"
#include "jdb_screen.h"
#include "kernel_console.h"
#include "keycodes.h"
#include "kernel_uart.h"
#include "kernel_task.h"
#include "kmem.h"
#include "koptions.h"
#include "logdefs.h"
#include "mem_layout.h"
#include "pic.h"
#include "push_console.h"
#include "processor.h"
#include "regdefs.h"
#include "static_init.h"
#include "terminate.h"
#include "thread.h"
#include "thread_state.h"
#include "timer.h"
#include "timer_tick.h"
#include "trap_state.h"
#include "vkey.h"
#include "watchdog.h"

char Jdb::_connected;			// Jdb::init() was done
// explicit single_step command
DEFINE_PER_CPU Per_cpu<char> Jdb::permanent_single_step;
volatile char Jdb::msr_test;		// = 1: trying to access an msr
volatile char Jdb::msr_fail;		// = 1: MSR access failed
DEFINE_PER_CPU Per_cpu<char> Jdb::code_ret; // current instruction is ret/iret
DEFINE_PER_CPU Per_cpu<char> Jdb::code_call;// current instruction is call
DEFINE_PER_CPU Per_cpu<char> Jdb::code_bra; // current instruction is jmp/jxx
DEFINE_PER_CPU Per_cpu<char> Jdb::code_int; // current instruction is int x

// special single step state
DEFINE_PER_CPU Per_cpu<Jdb::Step_state> Jdb::ss_state;
DEFINE_PER_CPU Per_cpu<int> Jdb::ss_level;  // current call level

const Unsigned8*Jdb::debug_ctrl_str;	// string+length for remote control of
int             Jdb::debug_ctrl_len;	// Jdb via enter_kdebugger("*#");

Unsigned16 Jdb::pic_status;
DEFINE_PER_CPU Per_cpu<unsigned> Jdb::apic_tpr;
DEFINE_PER_CPU Per_cpu<int> Jdb::jdb_irqs_disabled;

int  (*Jdb::bp_test_log_only)(Cpu_number cpu, Jdb_entry_frame *ef);
int  (*Jdb::bp_test_sstep)(Cpu_number cpu, Jdb_entry_frame *ef);
int  (*Jdb::bp_test_break)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);
int  (*Jdb::bp_test_other)(Cpu_number cpu, Jdb_entry_frame *ef, String_buffer *buf);

// available from the jdb_dump module
int jdb_dump_addr_task (Jdb_address addr, int level)
  __attribute__((weak));


STATIC_INITIALIZE_P(Jdb,JDB_INIT_PRIO);


IMPLEMENT FIASCO_INIT FIASCO_NOINLINE
void Jdb::init()
{
  if (Koptions::o()->opt(Koptions::F_nojdb))
    return;

  if (Koptions::o()->opt(Koptions::F_jdb_never_stop))
    never_break = 1;

  init_serial_console();

  Trap_state::base_handler = (Trap_state::Handler)enter_jdb;

  // if esc_hack, serial_esc or watchdog enabled, set slow timer handler
  Idt::set_vectors_run();

  // disable lbr feature per default since it eats cycles on AMD Athlon boxes
  Cpu::boot_cpu()->lbr_enable(false);

  Kconsole::console()->register_console(push_cons());

  _connected = true;
  Thread::may_enter_jdb = true;
}

PUBLIC static inline bool
Jdb::connected()
{
  return _connected;
}

IMPLEMENT_OVERRIDE inline template< typename T >
T
Jdb::monitor_address(Cpu_number current_cpu, T volatile const *addr)
{
  if (!*addr && Cpu::cpus.cpu(current_cpu).has_monitor_mwait())
    {
      asm volatile ("monitor \n" : : "a"(addr), "c"(0), "d"(0) );
      Mword irq_sup = Cpu::cpus.cpu(current_cpu).has_monitor_mwait_irq() ? 1 : 0;
      asm volatile ("mwait   \n" : : "a"(0x00), "c"(irq_sup) );
    }

  return *addr;
}


static inline
void
Jdb::backspace()
{
  putstr("\b \b");
}


DEFINE_PER_CPU static Per_cpu<Proc::Status> jdb_saved_flags;

// disable interrupts before entering the kernel debugger
IMPLEMENT
void
Jdb::save_disable_irqs(Cpu_number cpu)
{
  if (!jdb_irqs_disabled.cpu(cpu)++)
    {
      // save interrupt flags
      jdb_saved_flags.cpu(cpu) = Proc::cli_save();

      if (cpu == Cpu_number::boot_cpu())
	{
	  Watchdog::disable();
	  pic_status = Pic::disable_all_save();
          if (Config::getchar_does_hlt_works_ok)
            Timer_tick::disable(Cpu_number::boot_cpu());
	}
      if (Io_apic::active() && Apic::is_present())
	{
	  apic_tpr.cpu(cpu) = Apic::tpr();
	  Apic::tpr(APIC_IRQ_BASE - 0x08);
	}

      if (cpu == Cpu_number::boot_cpu() && Config::getchar_does_hlt_works_ok)
	{
	  // set timer interrupt does nothing than wakeup from hlt
	  Timer_tick::set_vectors_stop();
	  Timer_tick::enable(Cpu_number::boot_cpu());
	}

    }

  if (cpu == Cpu_number::boot_cpu() && Config::getchar_does_hlt_works_ok)
    // explicit enable interrupts because the timer interrupt is
    // needed to wakeup from "hlt" state in getchar(). All other
    // interrupts are disabled at the pic.
    Proc::sti();
}

// restore interrupts after leaving the kernel debugger
IMPLEMENT
void
Jdb::restore_irqs(Cpu_number cpu)
{
  if (!--jdb_irqs_disabled.cpu(cpu))
    {
      Proc::cli();

      if (Io_apic::active() && Apic::is_present())
	Apic::tpr(apic_tpr.cpu(cpu));

      if (cpu == Cpu_number::boot_cpu())
	{
	  Pic::restore_all(Jdb::pic_status);
	  Watchdog::enable();
	}

      // reset timer interrupt vector
      if (cpu == Cpu_number::boot_cpu() && Config::getchar_does_hlt_works_ok)
      	Idt::set_vectors_run();

      // reset interrupt flags
      Proc::sti_restore(jdb_saved_flags.cpu(cpu));
    }
}


struct On_dbg_stack
{
  Mword sp;
  On_dbg_stack(Mword sp) : sp(sp) {}
  bool operator () (Cpu_number cpu) const
  {
    Thread::Dbg_stack const &st = Thread::dbg_stack.cpu(cpu);
    return sp <= Mword(st.stack_top) 
       && sp >= Mword(st.stack_top) - Thread::Dbg_stack::Stack_size;
  }
};

// Do thread lookup using Trap_state. In contrast to Thread::current_thread()
// this function can also handle cases where we entered from kernel stack
// context. We _never_ return 0!
IMPLEMENT_OVERRIDE
Thread*
Jdb::get_thread(Cpu_number cpu)
{
  Jdb_entry_frame *entry_frame = Jdb::entry_frame.cpu(cpu);
  Address sp = (Address) entry_frame;

  // special case since we come from the double fault handler stack
  if (entry_frame->_trapno == 8 && !(entry_frame->cs() & 3))
    sp = entry_frame->sp(); // we can trust esp since it comes from main_tss

  if (foreach_cpu(On_dbg_stack(sp), false))
    return 0;

  if (!Helping_lock::threading_system_active)
    return 0;

  return static_cast<Thread*>(context_of((const void*)sp));
}

PUBLIC static inline
bool
Jdb::same_page(Address a1, Address a2)
{
  return (a1 & Config::PAGE_MASK) == (a2 & Config::PAGE_MASK);
}

PUBLIC static inline
bool
Jdb::consecutive_pages(Address a1, Address a2)
{
  return (a1 & Config::PAGE_MASK) + Config::PAGE_SIZE
      == (a2 & Config::PAGE_MASK);
}

PUBLIC static inline NEEDS[Jdb::same_page, Jdb::consecutive_pages]
bool
Jdb::same_or_consecutive_pages(Address a1, Address a2)
{
  return same_page(a1, a2) || consecutive_pages(a1, a2);
}

PUBLIC static
void
Jdb::peek_phys(Address phys, void *value, int width)
{
  assert(width > 0);
  assert(same_or_consecutive_pages(phys, phys + width - 1));

  Address virt = Kmem::map_phys_page_tmp(phys, 0);

  memcpy(value, (void*)virt, width);
}

PUBLIC static
void
Jdb::poke_phys(Address phys, void const *value, int width)
{
  assert(width > 0);
  assert(same_or_consecutive_pages(phys, phys + width - 1));

  Address virt = Kmem::map_phys_page_tmp(phys, 0);

  memcpy((void*)virt, value, width);
}

PRIVATE static
unsigned char *
Jdb::access_mem_task(Jdb_address addr, bool write)
{
  if (!Cpu::is_canonical_address(addr.addr()))
    return nullptr;

  Address phys;

  if (addr.is_kmem())
    {
      Address pdbr = Cpu::get_pdbr();
      Pdir *kdir = (Pdir *)Mem_layout::phys_to_pmem(pdbr);
      auto i = kdir->walk(Virt_addr(addr.addr()));
      if (!i.is_valid())
        return nullptr;

      if (!write || (i.attribs().rights & Page::Rights::W()))
        return (unsigned char *)addr.virt();

      phys = i.page_addr() | cxx::get_lsb(addr.addr(), i.page_order());
    }
  else if (addr.is_phys())
    phys = addr.phys();
  else
    {
      // user address, use temporary mapping
      phys = Address(addr.space()->virt_to_phys(addr.addr()));

#ifndef CONFIG_CPU_LOCAL_MAP
      if (phys == ~0UL)
        phys = addr.space()->virt_to_phys_s0(addr.virt());
#endif
    }

  if (phys == ~0UL)
    return nullptr;

  Address virt = Kmem::map_phys_page_tmp(phys, 0);
  return reinterpret_cast<unsigned char *>(virt);
}

PUBLIC static
Address
Jdb::get_phys_address(Jdb_address addr)
{
  if (addr.is_null())
    return (Address)~0UL;

  if (addr.is_phys())
    return addr.phys();

  if (addr.is_kmem())
    return Kmem::virt_to_phys(addr.virt());

  return addr.space()->virt_to_phys_s0(addr.virt());
}

// The content of apdapter memory is not shown by default because reading
// memory-mapped I/O registers may confuse the hardware. We assume that all
// memory above the end of the RAM is adapter memory.
PUBLIC static
int
Jdb::is_adapter_memory(Jdb_address addr)
{
  if (addr.is_null())
    return false;

  Address phys = get_phys_address(addr);

  if (phys == ~0UL)
    return false;

  for (auto const &m: Kip::k()->mem_descs_a())
    if (m.type() == Mem_desc::Conventional && !m.is_virtual()
        && m.contains(phys))
      return false;

  return true;
}

#define WEAK __attribute__((weak))
extern "C" char in_slowtrap, in_page_fault, in_handle_fputrap;
extern "C" char in_interrupt, in_timer_interrupt, in_timer_interrupt_slow;
extern "C" char in_slow_ipc4 WEAK, in_slow_ipc5;
extern "C" char in_sc_ipc1 WEAK, in_sc_ipc2 WEAK, in_syscall WEAK;
#undef WEAK

// Try to guess the thread state of t by walking down the kernel stack and
// locking at the first return address we find.
PUBLIC static
Jdb::Guessed_thread_state
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
	  if ((ktop[i] == (Mword)&in_slow_ipc4) ||  // entry.S, int 0x30 log
	      (ktop[i] == (Mword)&in_slow_ipc5) ||  // entry.S, sysenter log
#if defined (CONFIG_JDB_LOGGING)
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

  if (state == s_unknown && (t->state(false) & Thread_ipc_mask))
    state = s_ipc;

  return state;
}

PUBLIC static
void
Jdb::set_single_step(Cpu_number cpu, int on)
{
  if (on)
    entry_frame.cpu(cpu)->flags(entry_frame.cpu(cpu)->flags() | EFLAGS_TF);
  else
    entry_frame.cpu(cpu)->flags(entry_frame.cpu(cpu)->flags() & ~EFLAGS_TF);

  permanent_single_step.cpu(cpu) = on;
}


static bool
Jdb::handle_special_cmds(int c)
{
  foreach_cpu(&analyze_code);

  switch (c)
    {
    case 'j': // do restricted "go"
      switch (putchar(c=getchar()))
	{
	case 'b': // go until next branch
	case 'r': // go until current function returns
	  ss_level.cpu(current_cpu) = 0;
	  if (code_call.cpu(current_cpu))
	    {
	      // increase call level because currently we
	      // stay on a call instruction
	      ss_level.cpu(current_cpu)++;
	    }
	  ss_state.cpu(current_cpu) = (c == 'b') ? SS_BRANCH : SS_RETURN;
	  // if we have lbr feature, the processor treats the single
	  // step flag as step on branches instead of step on instruction
	  Cpu::boot_cpu()->btf_enable(true);
	  // fall through
	case 's': // do one single step
	  entry_frame.cpu(current_cpu)->flags(entry_frame.cpu(current_cpu)->flags() | EFLAGS_TF);
	  hide_statline = false;
	  return 0;
	default:
	  abort_command();
	  break;
	}
      break;
    default:
      backspace();
      // ignore character and get next input
      break;
    }

  return 1;

}


IMPLEMENTATION[ia32]:

// take a look at the code of the current thread eip
// set global indicators code_call, code_ret, code_bra, code_int
// This can fail if the current page is still not mapped
static void
Jdb::analyze_code(Cpu_number cpu)
{
  Jdb_entry_frame *entry_frame = Jdb::entry_frame.cpu(cpu);
  Space *task = get_task(cpu);
  // do nothing if page not mapped into this address space
  if (entry_frame->ip()+1 > Kmem::user_max())
    return;

  Unsigned8 op1, op2;

  Jdb_addr<Unsigned8> insn_ptr((Unsigned8*)entry_frame->ip(), task);

  if (   !peek(insn_ptr, op1)
      || !peek(insn_ptr + 1, op2))
    return;

  if (op1 != 0x0f && op1 != 0xff)
    op2 = 0;

  code_ret.cpu(cpu) =
	      (   ((op1 & 0xf6) == 0xc2)	// ret/lret /xxxx
	       || (op1 == 0xcf));		// iret

  code_call.cpu(cpu) =
	      (   (op1 == 0xe8)			// call near
	       || ((op1 == 0xff)
	           && ((op2 & 0x30) == 0x10))	// call/lcall *(...)
	       || (op1 == 0x9a));		// lcall xxxx:xxxx

  code_bra.cpu(cpu) =
	      (   ((op1 & 0xfc) == 0xe0)	// loop/jecxz
	       || ((op1 & 0xf0) == 0x70)	// jxx rel 8 bit
	       || (op1 == 0xeb)			// jmp rel 8 bit
	       || (op1 == 0xe9)			// jmp rel 16/32 bit
	       || ((op1 == 0x0f)
	           && ((op2 & 0xf0) == 0x80))	// jxx rel 16/32 bit
	       || ((op1 == 0xff)
	           && ((op2 & 0x30) == 0x20))	// jmp/ljmp *(...)
	       || (op1 == 0xea));		// ljmp xxxx:xxxx

  code_int.cpu(cpu) =
	      (   (op1 == 0xcc)			// int3
	       || (op1 == 0xcd)			// int xx
	       || (op1 == 0xce));		// into
}

IMPLEMENTATION[amd64]:

static void
Jdb::analyze_code(Cpu_number)
{}

IMPLEMENTATION[ia32,amd64]:

// entered debugger because of single step trap
static inline NOEXPORT int
Jdb::handle_single_step(Cpu_number cpu)
{
  int really_break = 1;

  analyze_code(cpu);

  Cpu const &ccpu = Cpu::cpus.cpu(cpu);
  error_buffer.cpu(cpu).clear();

  // special single_step ('j' command): go until branch/return
  if (ss_state.cpu(cpu) != SS_NONE)
    {
      if (ccpu.lbr_type() != Cpu::Lbr_unsupported)
	{
	  // don't worry, the CPU always knows what she is doing :-)
	}
      else
	{
	  // we have to emulate lbr looking at the code ...
	  switch (ss_state.cpu(cpu))
	    {
	    case SS_RETURN:
	      // go until function return
	      really_break = 0;
	      if (code_call.cpu(cpu))
		{
		  // increase call level
		  ss_level.cpu(cpu)++;
		}
	      else if (code_ret.cpu(cpu))
		{
		  // decrease call level
		  really_break = (ss_level.cpu(cpu)-- == 0);
		}
	      break;
	    case SS_BRANCH:
	    default:
	      // go until next branch
	      really_break = (code_ret.cpu(cpu) || code_call.cpu(cpu) || code_bra.cpu(cpu) || code_int.cpu(cpu));
	      break;
	    }
	}

      if (really_break)
	{
	  // condition met
	  ss_state.cpu(cpu) = SS_NONE;
	  error_buffer.cpu(cpu).printf("%s", "Branch/Call");
	}
    }
  else // (ss_state == SS_NONE)
    // regular single_step
    error_buffer.cpu(cpu).printf("%s", "Singlestep");

  return really_break;
}

// entered debugger due to debug exception
static inline NOEXPORT int
Jdb::handle_trap1(Cpu_number cpu, Jdb_entry_frame *ef)
{
  // FIXME: currently only on boot cpu
  if (cpu != Cpu_number::boot_cpu())
    return 0;

  if (bp_test_sstep && bp_test_sstep(cpu, ef))
    return handle_single_step(cpu);

  error_buffer.cpu(cpu).clear();
  if (bp_test_break
      && bp_test_break(cpu, ef, &error_buffer.cpu(cpu)))
    return 1;

  if (bp_test_other
      && bp_test_other(cpu, ef, &error_buffer.cpu(cpu)))
    return 1;

  return 0;
}

// entered debugger due to software breakpoint
static inline NOEXPORT bool
Jdb::handle_trap3(Cpu_number cpu, Jdb_entry_frame *ef)
{
  error_buffer.cpu(cpu).clear();

  if (ef->debug_entry_kernel_str())
    error_buffer.cpu(cpu).printf("%s", ef->text());
  else if (ef->debug_entry_user_str())
    error_buffer.cpu(cpu).printf("user \"%.*s\"", ef->textlen(), ef->text());

  return true;
}

// entered debugger due to other exception
static inline NOEXPORT int
Jdb::handle_trapX(Cpu_number cpu, Jdb_entry_frame *ef)
{
  error_buffer.cpu(cpu).clear();
  error_buffer.cpu(cpu).printf("%s", Cpu::exception_string(ef->_trapno));
  if (   ef->_trapno >= 10
      && ef->_trapno <= 14)
    error_buffer.cpu(cpu).printf("(ERR=" L4_PTR_FMT ")", ef->_err);

  return 1;
}

/** Int3 debugger interface. This function is called immediately
 * after entering the kernel debugger.
 * @return true if command was successfully interpreted
 */
IMPLEMENT
bool
Jdb::handle_user_request(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);

  if (ef->debug_ipi())
    return cpu != Cpu_number::boot_cpu();

  if (ef->debug_entry_kernel_sequence())
    return execute_command_ni(ef->text(), ef->textlen());

  return false;
}

IMPLEMENT
void
Jdb::enter_trap_handler(Cpu_number cpu)
{ Cpu::cpus.cpu(cpu).debugctl_disable(); }

IMPLEMENT
void
Jdb::leave_trap_handler(Cpu_number cpu)
{ Cpu::cpus.cpu(cpu).debugctl_enable(); }

IMPLEMENT
bool
Jdb::handle_conditional_breakpoint(Cpu_number cpu, Jdb_entry_frame *e)
{ return e->_trapno == 1 && bp_test_log_only && bp_test_log_only(cpu, e); }

IMPLEMENT
void
Jdb::handle_nested_trap(Jdb_entry_frame *e)
{
  // re-enable interrupts if we need them because they are disabled
  if (Config::getchar_does_hlt_works_ok)
    Proc::sti();

  switch (e->_trapno)
    {
    case 2:
      cursor(Jdb_screen::height(), 1);
      printf("\nNMI occured\n");
      break;
    case 3:
      cursor(Jdb_screen::height(), 1);
      printf("\nSoftware breakpoint inside jdb at " L4_PTR_FMT "\n",
             e->ip()-1);
      break;
    case 13:
      switch (msr_test)
	{
	case Msr_test_fail_warn:
	  printf(" MSR does not exist or invalid value\n");
	  msr_test = Msr_test_default;
	  msr_fail = 1;
	  break;
	case Msr_test_fail_ignore:
	  msr_test = Msr_test_default;
	  msr_fail = 1;
	  break;
	default:
	  cursor(Jdb_screen::height(), 1);
	  printf("\nGeneral Protection (eip=" L4_PTR_FMT ","
	      " err=" L4_PTR_FMT ", pfa=" L4_PTR_FMT ") -- jdb bug?\n",
	      e->ip(), e->_err, e->_cr2);
	  break;
	}
      break;
    default:
      cursor(Jdb_screen::height(), 1);
      printf("\nInvalid access (trap=%02lx err=" L4_PTR_FMT
	  " pfa=" L4_PTR_FMT " eip=" L4_PTR_FMT ") "
	  "-- jdb bug?\n",
	  e->_trapno, e->_err, e->_cr2, e->ip());
      break;
    }
}

IMPLEMENT
bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  bool really_break = true;
  auto *ef = entry_frame.cpu(cpu);

  if (ef->_trapno == 1)
    really_break = handle_trap1(cpu, ef);
  else if (ef->_trapno == 3)
    really_break = handle_trap3(cpu, ef);
  else
    really_break = handle_trapX(cpu, ef);

  if (really_break)
    {
      for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
	{
	  if (!Cpu::online(i) || !running.cpu(i))
	    continue;
	  // else S+ mode
	  if (!permanent_single_step.cpu(i))
	    entry_frame.cpu(i)->flags(entry_frame.cpu(i)->flags() & ~EFLAGS_TF);
	}
    }

  return really_break;
}

IMPLEMENT
bool
Jdb::test_checksums()
{ return Boot_info::get_checksum_ro() == Checksum::get_checksum_ro(); }

PUBLIC static inline
void
Jdb::enter_getchar()
{}

PUBLIC static inline
void
Jdb::leave_getchar()
{}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && mp]:

static
void
Jdb::send_nmi(Cpu_number cpu)
{
  Apic::mp_send_ipi(Apic::apic.cpu(cpu)->apic_id(), 0, Apic::APIC_IPI_NMI);
}
