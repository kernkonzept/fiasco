/*
 * Fiasco Thread Code
 * Shared between UX and native IA32.
 */
INTERFACE [ia32,amd64,ux]:

#include "trap_state.h"

class Idt_entry;

EXTENSION class Thread
{
private:
  /**
   * Return code segment used for exception reflection to user mode
   */
  static Mword exception_cs();

protected:
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};


//----------------------------------------------------------------------------
INTERFACE [ia32,amd64]:

class Trap_state;

EXTENSION class Thread
{
private:
  static int  (*int3_handler)(Trap_state*);
};


//----------------------------------------------------------------------------
IMPLEMENTATION [ia32,amd64,ux]:

#include "config.h"
#include "cpu.h"
#include "cpu_lock.h"
#include "gdt.h"
#include "idt.h"
#include "ipi.h"
#include "mem_layout.h"
#include "logdefs.h"
#include "paging.h"
#include "processor.h"		// for cli/sti
#include "regdefs.h"
#include "std_macros.h"
#include "thread.h"
#include "timer.h"
#include "trap_state.h"

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

IMPLEMENT
Thread::Thread()
: Receiver(),
  Sender(0),   // select optimized version of constructor
  _pager(Thread_ptr::Invalid),
  _exc_handler(Thread_ptr::Invalid),
  _del_observer(0)
{
  assert (state(false) == 0);

  inc_ref();
  _space.space(Kernel_task::kernel_task());

  if (Config::Stack_depth)
    std::memset((char*)this + sizeof(Thread), '5',
		Thread::Size-sizeof(Thread)-64);

  _magic          = magic;
  _recover_jmpbuf = 0;
  _timeout        = 0;

  *reinterpret_cast<void(**)()> (--_kernel_sp) = user_invoke;

  arch_init();

  state_add_dirty(Thread_dead, false);

  // ok, we're ready to go!
}

IMPLEMENT inline NEEDS[Thread::exception_triggered]
Mword
Thread::user_ip() const
{ return exception_triggered()?_exc_cont.ip():regs()->ip(); }

IMPLEMENT inline
Mword
Thread::user_flags() const
{ return regs()->flags(); }

PRIVATE static inline
Mword
Thread::sanitize_user_flags(Mword flags)
{ return (flags & ~(EFLAGS_IOPL | EFLAGS_NT)) | EFLAGS_IF; }

/** Check if the pagefault occurred at a special place: At some places in the
    kernel we want to ensure that a specific address is mapped. The regular
    case is "mapped", the exception or slow case is "not mapped". The fastest
    way to check this is to touch into the memory. If there is no mapping for
    the address we get a pagefault. Now the pagefault exception handler can
    recognize that situation by scanning the code. The trick is that the
    assembler instruction "andl $0xffffffff, %ss:(%ecx)" _clears_ the carry
    flag normally (see Intel reference manual). The pager wants to inform the
    code that there was a pagefault and therefore _sets_ the carry flag. So
    the code has only to check if the carry flag is set. If yes, there was
    a pagefault at this instruction.
    @param ip pagefault address */
IMPLEMENT inline
bool
Thread::pagein_tcb_request(Return_frame *regs)
{
  unsigned long new_ip = regs->ip();
  if (*(Unsigned8*)new_ip == 0x48) // REX.W
    new_ip += 1;

  Unsigned16 op = *(Unsigned16*)new_ip;
  //LOG_MSG_3VAL(current(),"TCB", op, new_ip, 0);
  if ((op & 0xc0ff) == 0x8b) // Context::is_tcb_mapped() and Context::state()
    {
      regs->ip(new_ip + 2);
      // stack layout:
      //         user eip
      //         PF error code
      // reg =>  eax/rax
      //         ecx/rcx
      //         edx/rdx
      //         ...
      Mword *reg = ((Mword*)regs) - 2 - Return_frame::Pf_ax_offset;
#if 0
      LOG_MSG_3VAL(current(),"TCB", op, regs->ip(), (Mword)reg);
      LOG_MSG_3VAL(current(),"TCBX", reg[-3], reg[-4], reg[-5]);
      LOG_MSG_3VAL(current(),"TCB0", reg[0], reg[-1], reg[-2]);
      LOG_MSG_3VAL(current(),"TCB1", reg[1], reg[2], reg[3]);
#endif
      assert((op >> 11) <= 2);
      reg[-(op>>11)] = 0; // op==0 => eax, op==1 => ecx, op==2 => edx

      // tell program that a pagefault occured we cannot handle
      regs->flags(regs->flags() | 0x41); // set carry and zero flag in EFLAGS
      return true;
    }
  else if (*(Unsigned32*)regs->ip() == 0xff01f636) // used in shortcut.S
    {
      regs->ip(regs->ip() + 4);
      regs->flags(regs->flags() | 1);  // set carry flag in EFLAGS
      return true;
    }

  return false;
}


extern "C" FIASCO_FASTCALL
void
thread_restore_exc_state()
{
  current_thread()->restore_exc_state();
}

PRIVATE static
void
Thread::print_page_fault_error(Mword e)
{
  printf("%lx", e);
}

/**
 * The global trap handler switch.
 * This function handles CPU-exception reflection, int3 debug messages,
 * kernel-debugger invocation, and thread crashes (if a trap cannot be
 * handled).
 * @param state trap state
 * @return 0 if trap has been consumed by handler;
 *          -1 if trap could not be handled.
 */
PUBLIC
int
Thread::handle_slow_trap(Trap_state *ts)
{
  Address ip;
  int from_user = ts->cs() & 3;

  if (EXPECT_FALSE(ts->_trapno == 0xee)) //debug IPI
    {
      Ipi::eoi(Ipi::Debug, current_cpu());
      goto generic_debug;
    }

  if (from_user && _space.user_mode())
    {
      if (ts->_trapno == 14 && Kmem::is_io_bitmap_page_fault(ts->_cr2))
        {
	  ts->_trapno = 13;
	  ts->_err    = 0;
        }

      if (send_exception(ts))
	goto success;
    }

  // XXX We might be forced to raise an exception. In this case, our return
  // CS:IP points to leave_by_trigger_exception() which will trigger the
  // exception just before returning to userland. But if we were inside an
  // IPC while we was ex-regs'd, we will generate the 'exception after the
  // syscall' _before_ we leave the kernel.
  if (ts->_trapno == 13 && (ts->_err & 6) == 6)
    goto check_exception;

  LOG_TRAP;

  if (!check_trap13_kernel (ts))
    return 0;

  if (EXPECT_FALSE(!from_user))
    {
      // get also here if a pagefault was not handled by the user level pager
      if (ts->_trapno == 14)
	goto check_exception;

      goto generic_debug;      // we were in kernel mode -- nothing to emulate
    }

  if (EXPECT_FALSE(ts->_trapno == 2))
    goto generic_debug;        // NMI always enters kernel debugger

  if (EXPECT_FALSE(ts->_trapno == 0xffffffff))
    goto generic_debug;        // debugger interrupt

  check_f00f_bug(ts);

  // so we were in user mode -- look for something to emulate

  // We continue running with interrupts off -- no sti() here. But
  // interrupts may be enabled by the pagefault handler if we get a
  // pagefault in peek_user().

  // Set up exception handling.  If we suffer an un-handled user-space
  // page fault, kill the thread.
  jmp_buf pf_recovery;
  unsigned error;
  if (EXPECT_FALSE ((error = setjmp(pf_recovery)) != 0) )
    {
      WARN("%p killed:\n"
           "\033[1mUnhandled page fault, code=%08x\033[m\n", this, error);
      goto fail_nomsg;
    }

  _recover_jmpbuf = &pf_recovery;

  switch (handle_io_page_fault(ts))
    {
    case 1:
      _recover_jmpbuf = 0;
      goto success;
    case 2:
      _recover_jmpbuf = 0;
      goto fail;
    default:
      break;
    }

  ip = ts->ip();

  // just print out some warning, we do the normal exception handling
  handle_sysenter_trap(ts, ip, from_user);
  _recover_jmpbuf = 0;

check_exception:

  // send exception IPC if requested
  if (send_exception(ts))
    goto success;

  // backward compatibility cruft: check for those insane "int3" debug
  // messaging command sequences
  if (ts->_trapno == 3)
    {
      if (int3_handler && int3_handler(ts))
	goto success;

      goto generic_debug;
    }

  // privileged tasks also may invoke the kernel debugger with a debug
  // exception
  if (ts->_trapno == 1)
    goto generic_debug;


fail:
  // can't handle trap -- kill the thread
  WARN("%p killed:\n"
       "\033[1mUnhandled trap \033[m\n", this);

fail_nomsg:
  if ((int)Config::Warn_level >= Warning)
    ts->dump();

  halt();

success:
  _recover_jmpbuf = 0;
  return 0;

generic_debug:
  _recover_jmpbuf = 0;

  if (!nested_trap_handler)
    return handle_not_nested_trap(ts);

  return call_nested_trap_handler(ts);
}

/**
 * The low-level page fault handler called from entry.S.  We're invoked with
 * interrupts turned off.  Apart from turning on interrupts in almost
 * all cases (except for kernel page faults in TCB area), just forwards
 * the call to Thread::handle_page_fault().
 * @param pfa page-fault virtual address
 * @param error_code CPU error code
 * @return true if page fault could be resolved, false otherwise
 */
extern "C" FIASCO_FASTCALL
int
thread_page_fault(Address pfa, Mword error_code, Address ip, Mword flags,
		  Return_frame *regs)
{

  // XXX: need to do in a different way, if on debug stack e.g.
#if 0
  // If we're in the GDB stub -- let generic handler handle it
  if (EXPECT_FALSE (!in_context_area((void*)Proc::stack_pointer())))
    return false;
#endif

  Thread *t = current_thread();
  // Pagefault in user mode or interrupts were enabled
  if (EXPECT_TRUE(PF::is_usermode_error(error_code))
      && t->vcpu_pagefault(pfa, error_code, ip))
    return 1;

  if(EXPECT_TRUE(PF::is_usermode_error(error_code))
     || (flags & EFLAGS_IF)
     || !Kmem::is_kmem_page_fault(pfa, error_code))
    Proc::sti();

  return t->handle_page_fault(pfa, error_code, ip, regs);
}

/** The catch-all trap entry point.  Called by assembly code when a 
    CPU trap (that's not specially handled, such as system calls) occurs.
    Just forwards the call to Thread::handle_slow_trap().
    @param state trap state
    @return 0 if trap has been consumed by handler;
           -1 if trap could not be handled.
 */
extern "C" FIASCO_FASTCALL
int
thread_handle_trap(Trap_state *ts, unsigned)
{
  return current_thread()->handle_slow_trap(ts);
}


//
// Public services
//

IMPLEMENT inline
bool
Thread::handle_sigma0_page_fault(Address pfa)
{
  Mem_space::Page_order size = mem_space()->largest_page_size(); // take a page size less than 16MB (1<<24)
  auto f = mem_space()->fitting_sizes();
  Virt_addr va = Virt_addr(pfa);

  // Check if mapping a superpage doesn't exceed the size of physical memory
  // Some distributions do not allow to mmap below a certain threshold
  // (like 64k on Ubuntu 8.04) so we cannot map a superpage at 0 if
  // we're Fiasco-UX
  while (Config::Is_ux && (va < (Virt_addr(1) << size)))
    size = f(--size);

  va = cxx::mask_lsb(va, size);

  return mem_space()->v_insert(Mem_space::Phys_addr(va), va, size,
                               Page::Attr(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}

PRIVATE static inline
void
Thread::save_fpu_state_to_utcb(Trap_state *, Utcb *)
{}

/* return 1 if this exception should be sent, return 0 if not
 */
PUBLIC inline NEEDS["trap_state.h"]
int
Thread::send_exception_arch(Trap_state *ts)
{
  // Do not send exception IPC but return 'not for us' if thread is a normal
  // thread (not alien) and it's a debug trap,
  // debug traps for aliens are always reflected as exception IPCs
  if (!(state() & Thread_alien)
      && (ts->_trapno == 1 || ts->_trapno == 3))
    return 0; // we do not handle this

  if (ts->_trapno == 3)
    {
      if (state() & Thread_dis_alien)
	{
	  state_del(Thread_dis_alien);
	  return 0; // no exception
	}

      // set IP back on the int3 instruction
      ts->ip(ts->ip() - 1);
    }

  return 1; // make it an exception
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!(vmx || svm) && (ia32 || amd64)]:

PRIVATE inline void Thread::_hw_virt_arch_init_vcpu_state(Vcpu_state *) {}

//----------------------------------------------------------------------------
IMPLEMENTATION [(vmx || svm) && (ia32 || amd64)]:

#include "vmx.h"
#include "svm.h"

PRIVATE inline NEEDS["vmx.h", "svm.h"]
void
Thread::_hw_virt_arch_init_vcpu_state(Vcpu_state *vcpu_state)
{
  if (Vmx::cpus.current().vmx_enabled())
    Vmx::cpus.current().init_vmcs_infos(vcpu_state);

  // currently we do nothing for SVM here
}

IMPLEMENT_OVERRIDE
bool
Thread::arch_ext_vcpu_enabled()
{
  return Vmx::cpus.current().vmx_enabled() || Svm::cpus.current().svm_enabled();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32]:

IMPLEMENT_OVERRIDE inline NEEDS[Thread::_hw_virt_arch_init_vcpu_state]
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;

  if (ext)
    _hw_virt_arch_init_vcpu_state(vcpu_state);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

IMPLEMENT_OVERRIDE inline NEEDS[Thread::_hw_virt_arch_init_vcpu_state]
void
Thread::arch_init_vcpu_state(Vcpu_state *vcpu_state, bool ext)
{
  vcpu_state->version = Vcpu_arch_version;
  vcpu_state->host.fs_base = _fs_base;
  vcpu_state->host.gs_base = _gs_base;
  vcpu_state->host.ds = 0;
  vcpu_state->host.es = 0;
  vcpu_state->host.fs = 0;
  vcpu_state->host.ds = 0;
  vcpu_state->host.user_ds32 = Gdt::gdt_data_user | Gdt::Selector_user;
  vcpu_state->host.user_cs64 = Gdt::gdt_code_user | Gdt::Selector_user;
  vcpu_state->host.user_cs32 = Gdt::gdt_code_user32 | Gdt::Selector_user;

  if (ext)
    _hw_virt_arch_init_vcpu_state(vcpu_state);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

#include <feature.h>
KIP_KERNEL_FEATURE("segments");

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{}

PRIVATE inline
L4_msg_tag
Thread::sys_gdt_x86(L4_msg_tag tag, Utcb *utcb)
{
  // if no words given then return the first gdt entry
  if (EXPECT_FALSE(tag.words() == 1))
    {
      utcb->values[0] = Gdt::gdt_user_entry1 >> 3;
      return Kobject_iface::commit_result(0, 1);
    }

  unsigned idx = 0;

  for (unsigned entry_number = utcb->values[1];
      entry_number < Gdt_user_entries
      && Utcb::val_idx(Utcb::val64_idx(2) + idx) < tag.words();
      ++idx, ++entry_number)
    {
      Gdt_entry d = access_once((Gdt_entry *)&utcb->val64[Utcb::val64_idx(2) + idx]);
      if (!d.unsafe())
        _gdt_user_entries[entry_number] = d;
    }

  if (this == current_thread())
    load_gdt_user_entries();

  return Kobject_iface::commit_result((utcb->values[1] << 3) + Gdt::gdt_user_entry1 + 3);
}




//----------------------------------------------------------------------------
IMPLEMENTATION [ux]:

PROTECTED inline
void
Thread::vcpu_resume_user_arch()
{
  load_gdt_user_entries();
}

//----------------------------------------------------------------------------
IMPLEMENTATION [ux || amd64]:

IMPLEMENT inline NEEDS[Thread::exception_triggered]
void
Thread::user_ip(Mword ip)
{
  if (exception_triggered())
    _exc_cont.ip(ip);
  else
    {
      Entry_frame *r = regs();
      r->ip(ip);
    }
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32,amd64,ux) && !io]:

PRIVATE inline
int
Thread::handle_io_page_fault(Trap_state *)
{ return 0; }

PRIVATE inline
bool
Thread::get_ioport(Address /*eip*/, Trap_state * /*ts*/,
                   unsigned * /*port*/, unsigned * /*size*/)
{ return false; }


//---------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include "fpu.h"
#include "fpu_alloc.h"
#include "fpu_state.h"
#include "gdt.h"
#include "globalconfig.h"
#include "idt.h"
#include "simpleio.h"
#include "static_init.h"
#include "terminate.h"

int (*Thread::int3_handler)(Trap_state*);
DEFINE_PER_CPU Per_cpu<Thread::Dbg_stack> Thread::dbg_stack;

STATIC_INITIALIZER_P (int3_handler_init, KDB_INIT_PRIO);

static
void
int3_handler_init()
{
  Thread::set_int3_handler(Thread::handle_int3);
}

IMPLEMENT static inline NEEDS ["gdt.h"]
Mword
Thread::exception_cs()
{
  return Gdt::gdt_code_user | Gdt::Selector_user;
}

/**
 * The ia32 specific part of the thread constructor.
 */
PRIVATE inline NEEDS ["gdt.h"]
void
Thread::arch_init()
{
  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  r->flags(EFLAGS_IOPL_K | EFLAGS_IF | 2);	// ei
  r->cs(Gdt::gdt_code_user | Gdt::Selector_user);
  r->ss(Gdt::gdt_data_user | Gdt::Selector_user);

  r->sp(0);
  // after cs initialisation as ip() requires proper cs
  r->ip(0);
}


/** A C interface for Context::handle_fpu_trap, callable from assembly code.
    @relates Context
 */
// The "FPU not available" trap entry point
extern "C"
int
thread_handle_fputrap()
{
  LOG_TRAP_N(7);

  return current_thread()->switchin_fpu();
}

PUBLIC static inline
void
Thread::set_int3_handler(int (*handler)(Trap_state *ts))
{
  int3_handler = handler;
}

/**
 * Default handle for int3 extensions if JDB is disabled. If the JDB is
 * available, Jdb::handle_int3_threadctx is called instead.
 * @return 0 not handled, wait for user response
 *         1 successfully handled
 */
PUBLIC static
int
Thread::handle_int3(Trap_state *ts)
{
  Mem_space *s   = current()->mem_space();
  int from_user  = ts->cs() & 3;
  Address   ip   = ts->ip();
  Unsigned8 todo = s->peek((Unsigned8*)ip, from_user);
  Unsigned8 *str;
  int len;
  char c;

  switch (todo)
    {
    case 0xeb: // jmp == enter_kdebug()
      len = s->peek((Unsigned8*)(ip+1), from_user);
      str = (Unsigned8*)(ip + 2);

      putstr("KDB: ");
      if (len > 0)
	{
	  for (; len; len--)
            putchar(s->peek(str++, from_user));
	}
      putchar('\n');
      return 0; // => Jdb

    case 0x90: // nop == l4kd_display()
      if (          s->peek((Unsigned8*)(ip+1), from_user)  != 0xeb /*jmp*/
	  || (len = s->peek((Unsigned8*)(ip+2), from_user)) <= 0)
	return 0; // => Jdb

      str = (Unsigned8*)(ip + 3);
      for (; len; len--)
	putchar(s->peek(str++, from_user));
      break;

    case 0x3c: // cmpb
      todo = s->peek((Unsigned8*)(ip+1), from_user);
      switch (todo)
	{
	case  0: // l4kd_outchar
	  putchar(ts->value() & 0xff);
	  break;
        case  1: // l4kd_outnstring
	  str = (Unsigned8*)ts->value();
          len = ts->value4();
	  for(; len > 0; len--)
	    putchar(s->peek(str++, from_user));
	  break;
	case  2: // l4kd_outstr
	  str = (Unsigned8*)ts->value();
	  for (; (c=s->peek(str++, from_user)); )
            putchar(c);
	  break;
	case  5: // l4kd_outhex32
	  printf("%08lx", ts->value() & 0xffffffff);
	  break;
	case  6: // l4kd_outhex20
	  printf("%05lx", ts->value() & 0xfffff);
	  break;
	case  7: // l4kd_outhex16
	  printf("%04lx", ts->value() & 0xffff);
	  break;
	case  8: // l4kd_outhex12
	  printf("%03lx", ts->value() & 0xfff);
	  break;
	case  9: // l4kd_outhex8
	  printf("%02lx", ts->value() & 0xff);
	  break;
	case 11: // l4kd_outdec
	  printf("%lu", ts->value());
	  break;
	case 31: // Watchdog
	  switch (ts->value2())
	    {
	    case 1:
	      // enable watchdog
	      Watchdog::user_enable();
	      break;
	    case 2:
	      // disable watchdog
	      Watchdog::user_disable();
	      break;
	    case 3:
	      // user takes over the control of watchdog and is from now on
	      // responsible for calling "I'm still alive" events (function 5)
	      Watchdog::user_takeover_control();
	      break;
	    case 4:
	      // user returns control of watchdog to kernel
	      Watchdog::user_giveback_control();
              break;
	    case 5:
	      // I'm still alive
	      Watchdog::touch();
	      break;
	    }
	  break;

	default: // ko
	  if (todo < ' ')
	    return 0; // => Jdb

	  putchar(todo);
	  break;
	}
      break;

    default:
      return 0; // => Jdb
    }

  return 1;
}


PRIVATE inline
void
Thread::check_f00f_bug(Trap_state *ts)
{
  // If we page fault on the IDT, it must be because of the F00F bug.
  // Figure out exception slot and raise the corresponding exception.
  // XXX: Should we also modify the error code?
  if (ts->_trapno == 14		// page fault?
      && ts->_cr2 >= Idt::idt()
      && ts->_cr2 <  Idt::idt() + Idt::_idt_max * 8)
    ts->_trapno = (ts->_cr2 - Idt::idt()) / 8;
}


PRIVATE inline
unsigned
Thread::check_io_bitmap_delimiter_fault(Trap_state *ts)
{
  // check for page fault at the byte following the IO bitmap
  if (ts->_trapno == 14           // page fault?
      && (ts->_err & 4) == 0         // in supervisor mode?
      && ts->ip() <= Mem_layout::User_max   // delimiter byte accessed?
      && (ts->_cr2 == Mem_layout::Io_bitmap + Mem_layout::Io_port_max / 8))
    {
      // page fault in the first byte following the IO bitmap
      // map in the cpu_page read_only at the place
      Mem_space::Status result =
	mem_space()->v_insert(
	    Mem_space::Phys_addr(mem_space()->virt_to_phys_s0((void*)Kmem::io_bitmap_delimiter_page())),
	    Virt_addr(Mem_layout::Io_bitmap + Mem_layout::Io_port_max / 8),
	    Mem_space::Page_order(Config::PAGE_SHIFT),
	    Page::Attr(Page::Rights::R(), Page::Type::Normal(), Page::Kern::Global()));

      switch (result)
	{
	case Mem_space::Insert_ok:
	  return 1; // success
	case Mem_space::Insert_err_nomem:
	  // kernel failure, translate this into a general protection
	  // violation and hope that somebody handles it
	  ts->_trapno = 13;
	  ts->_err    =  0;
	  return 0; // fail
	default:
	  // no other error code possible
	  assert (false);
	}
    }

  return 1;
}

PRIVATE inline
bool
Thread::handle_sysenter_trap(Trap_state *ts, Address eip, bool from_user)
{
  if (EXPECT_FALSE
      ((ts->_trapno == 6 || ts->_trapno == 13)
       && (ts->_err & 0xffff) == 0
       && (eip < Mem_layout::User_max - 1)
       && (mem_space()->peek((Unsigned16*) eip, from_user)) == 0x340f))
    {
      // somebody tried to do sysenter on a machine without support for it
      WARN("tcb=%p killed:\n"
	   "\033[1;31mSYSENTER not supported on this machine\033[0m",
	   this);

      if (Cpu::have_sysenter())
	// GP exception if sysenter is not correctly set up..
        WARN("MSR_SYSENTER_CS: %llx", Cpu::rdmsr(MSR_SYSENTER_CS));
      else
	// We get UD exception on processors without SYSENTER/SYSEXIT.
        WARN("SYSENTER/EXIT not available.");

      return false;
    }

  return true;
}

PRIVATE inline
int
Thread::handle_not_nested_trap(Trap_state *ts)
{
  // no kernel debugger present
  printf(" %p IP=" L4_PTR_FMT " Trap=%02lx [Ret/Esc]\n",
	 this, ts->ip(), ts->_trapno);

  int r;
  // cannot use normal getchar because it may block with hlt and irq's
  // are off here
  while ((r = Kconsole::console()->getchar(false)) == -1)
    Proc::pause();

  if (r == '\033')
    terminate (1);

  return 0;
}

PROTECTED inline
int
Thread::sys_control_arch(Utcb *)
{
  return 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 | amd64) & (debug | kdb) & !mp]:

PRIVATE static inline Cpu_number Thread::dbg_find_cpu() { return Cpu_number::boot_cpu(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 | amd64) & (debug | kdb) & mp]:

#include "apic.h"

PRIVATE static inline NEEDS["apic.h"]
Cpu_number
Thread::dbg_find_cpu()
{
  unsigned long phys_cpu = Apic::get_id();
  Cpu_number log_cpu = Apic::find_cpu(phys_cpu);
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%lx\n", phys_cpu);
      log_cpu = Cpu_number::boot_cpu();
    }
  return log_cpu;
}


//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 |amd64) & !(debug | kdb)]:

/** There is no nested trap handler if both jdb and kdb are disabled.
 * Important: We don't need the nested_handler_stack here.
 */
PRIVATE static inline
int
Thread::call_nested_trap_handler(Trap_state *)
{ return -1; }
