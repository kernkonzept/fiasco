/*
 * Fiasco Thread Code
 */
INTERFACE [ia32 || amd64]:

#include "trap_state.h"

class Idt_entry;

EXTENSION class Thread
{
private:
  enum { ShowDebugMessages = 0 };

  /**
   * Return value for \ref handle_io_fault.
   */
  enum Io_return
  {
    Ignored = 0,
    Retry = 1,
    Fail = 2
  };

  /**
   * Return code segment used for exception reflection to user mode
   */
  static Mword exception_cs();

protected:
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

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
#include "exc_table.h"

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

IMPLEMENT
Thread::Thread(Ram_quota *q)
: Receiver(),
  Sender(0),   // select optimized version of constructor
  _pager(Thread_ptr::Invalid),
  _exc_handler(Thread_ptr::Invalid),
  _quota(q),
  _del_observer(0)
{
  assert (state(false) == 0);

  inc_ref();
  _space.space(Kernel_task::kernel_task());

  if constexpr (Config::Stack_depth)
    std::memset(reinterpret_cast<char *>(this) + sizeof(Thread), '5',
                Thread::Size - sizeof(Thread) - 64);

  _magic = magic;
  _timeout = 0;
  clear_recover_jmpbuf();

  prepare_switch_to(&user_invoke);

  arch_init();

  alloc_eager_fpu_state();

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

PRIVATE static
bool
Thread::handle_kernel_exc(Trap_state *ts)
{
  for (auto const &e: Exc_entry::table())
    {
      if (e.ip != ts->ip()) continue;

      if (e.handler)
        {
          if constexpr (ShowDebugMessages)
            printf("fixup exception(h): %ld: "
                   "ip=%lx -> handler %p fixup %lx ts @ %p -> %lx\n",
                   ts->trapno(), ts->ip(), reinterpret_cast<void *>(e.handler),
                   e.fixup, static_cast<void *>(ts),
                   reinterpret_cast<Mword const *>(ts)[-1]);
          if constexpr (ShowDebugMessages)
            ts->dump();

          return e.handler(&e, ts);
        }
      else if (e.fixup)
        {
          if constexpr (ShowDebugMessages)
            printf("fixup exception: %ld: ip=%lx -> fixup %lx\n",
                   ts->trapno(), ts->ip(), e.fixup);
          ts->ip(e.fixup);
          return true;
        }
      else
        return false;
    }
  return false;
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
  int from_user = ts->cs() & 3;
  Io_return res_iopf;

  if (EXPECT_FALSE(ts->_trapno == 0xee)) //debug IPI
    {
      Ipi::eoi(Ipi::Debug, current_cpu());
      goto generic_debug;
    }

  if (EXPECT_FALSE(ts->_trapno == 2))
    goto generic_debug;        // NMI always enters kernel debugger

  if (from_user && _space.user_mode())
    {
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

  if (!check_trap13_kernel(ts))
    return 0;

  if (EXPECT_FALSE(!from_user))
    {
      if (!check_unknown_irq(ts))
        return 0;

      if (handle_kernel_exc(ts))
        goto success;

      // get also here if a pagefault was not handled by the user level pager
      if (ts->_trapno == 14)
        {
          if ((ts->_err & (PF_ERR_INSTFETCH | PF_ERR_PRESENT))
              == (PF_ERR_INSTFETCH | PF_ERR_PRESENT))
            {
              ts->dump();
              panic("kernel data execution\n");
            }
          if ((ts->_err & (PF_ERR_WRITE | PF_ERR_PRESENT))
              == (PF_ERR_WRITE | PF_ERR_PRESENT))
            {
              ts->dump();
              panic("kernel code modification\n");
            }
          goto check_exception;
        }

      goto generic_debug;      // we were in kernel mode -- nothing to emulate
    }

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
           "\033[1mUnhandled page fault, code=%08x\033[m\n",
           static_cast<void *>(this), error);
      goto fail_nomsg;
    }

  set_recover_jmpbuf(&pf_recovery);
  res_iopf = handle_io_fault(ts);
  clear_recover_jmpbuf();

  switch (res_iopf)
    {
    case Ignored:
      break;
    case Retry:
      goto success;
    case Fail:
      goto fail;
    }

check_exception:
  // see kdb_ke(), kdb_ke_nstr(), kdb_ke_nsequence()
  if (!from_user && (ts->_trapno == 3))
    goto generic_debug;

  // send exception IPC if requested
  if (send_exception(ts))
    goto success;

  // privileged tasks also may invoke the kernel debugger with a debug
  // exception
  if (ts->_trapno == 1)
    goto generic_debug;


fail:
  // can't handle trap -- kill the thread
  WARN("%p killed:\n"
       "\033[1mUnhandled trap \033[m\n", static_cast<void *>(this));

fail_nomsg:
  if (Warn::is_enabled(Warning))
    ts->dump();

  kill();
  return 0;

success:
  clear_recover_jmpbuf();
  return 0;

generic_debug:
  clear_recover_jmpbuf();

  if (!nested_trap_handler)
    return handle_not_nested_trap(ts);

  return call_nested_trap_handler(ts);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && cpu_local_map]:

PUBLIC inline
bool
Thread::update_local_map(Address pfa, Mword /*error_code*/)
{
  // This function assumes 4-level paging on AMD64. The page map level 4 table
  // is indexed by bits 47..39 of a linear address. Thus each entry covers 512G.
  static_assert(255 == (Mem_layout::User_max >> 39),
                "Mem_layout::User_max must lie in 512G slot 255.");
  // 512G slot 259 is used for context-specific kernel data.
  static_assert(259 == ((Mem_layout::Caps_start >> 39) & 0x1ff),
                "Mem_layout::Caps_start must lie in 512G slot 259.");
  static_assert(259 == (((Mem_layout::Caps_end - 1) >> 39) & 0x1ff),
                "Mem_layout::Caps_end - 1 must lie in 512G slot 259.");

  unsigned idx = (pfa >> 39) & 0x1ff;
  if (EXPECT_FALSE((idx > 255) && idx != 259))
    return false;

  auto *m = Kmem::pte_map();
  if (EXPECT_FALSE((*m)[idx]))
    return false;

  auto s = Kmem::current_cpu_udir()->walk(Virt_addr(pfa), 0);
  assert (!s.is_valid());
  auto r = vcpu_aware_space()->dir()->walk(Virt_addr(pfa), 0);
  if (EXPECT_FALSE(!r.is_valid()))
    return false;

  m->set_bit(idx);
  *s.pte = *r.pte;
  return true;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && !cpu_local_map]:

PUBLIC inline
bool
Thread::update_local_map(Address, Mword)
{ return false; }

//----------------------------------------------------------------------------
IMPLEMENTATION [ia32 || amd64]:

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

  if (t->update_local_map(pfa, error_code))
    return 1;

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
thread_handle_trap(Trap_state *ts, Cpu_number)
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
  Virt_addr va = Virt_addr(pfa);

  va = cxx::mask_lsb(va, size);

  return mem_space()->v_insert(Mem_space::Phys_addr(va), va, size,
                               Page::Attr::space_local(L4_fpage::Rights::URWX()))
    != Mem_space::Insert_err_nomem;
}

//----------------------------------------------------------------------------
IMPLEMENTATION [!(vmx || svm) && (ia32 || amd64)]:

PRIVATE inline void Thread::_hw_virt_arch_init_vcpu_state(Vcpu_state *) {}

//----------------------------------------------------------------------------
IMPLEMENTATION [(vmx || svm) && (ia32 || amd64)]:

#include "vmx.h"
#include "svm.h"

PRIVATE inline NEEDS["vmx.h", "svm.h", "cpu.h"]
void
Thread::_hw_virt_arch_init_vcpu_state(Vcpu_state *vcpu_state)
{
  if (Vmx::cpus.current().vmx_enabled())
    Vmx::cpus.current().init_vcpu_state(vcpu_state);

  if (Cpu::boot_cpu()->vendor() == Cpu::Vendor_intel)
    vcpu_state->user_data[6] = Cpu::ucode_revision();

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
  vcpu_state->host.gs = 0;
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

PRIVATE inline
L4_msg_tag
Thread::sys_gdt_x86(L4_msg_tag tag, Utcb const *utcb, Utcb *out)
{
  // if no words given then return the first gdt entry
  if (EXPECT_FALSE(tag.words() == 1))
    {
      out->values[0] = Gdt::gdt_user_entry1 >> 3;
      return Kobject_iface::commit_result(0, 1);
    }

  unsigned idx = 0;

  for (unsigned entry_number = utcb->values[1];
      entry_number < Gdt_user_entries
      && Utcb::val_idx(Utcb::val64_idx(2) + idx) < tag.words();
      ++idx, ++entry_number)
    {
      Gdt_entry d = access_once(reinterpret_cast<Gdt_entry const *>(
                                  &utcb->val64[Utcb::val64_idx(2) + idx]));
      if (d.unsafe())
        return Kobject_iface::commit_result(-L4_err::EInval);

      _gdt_user_entries[entry_number] = d;
    }

  if (this == current_thread())
    load_gdt_user_entries();

  return Kobject_iface::commit_result((utcb->values[1] << 3) + Gdt::gdt_user_entry1 + 3);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [amd64]:

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
IMPLEMENTATION [(ia32 || amd64) && !io]:

PRIVATE inline
Thread::Io_return
Thread::handle_io_fault(Trap_state *)
{
  return Ignored;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

#include "apic.h"
#include "fpu.h"
#include "fpu_alloc.h"
#include "fpu_state_ptr.h"
#include "gdt.h"
#include "globalconfig.h"
#include "idt.h"
#include "keycodes.h"
#include "simpleio.h"
#include "static_init.h"
#include "terminate.h"

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

/**
 * Chech whether the current trap is #GP caused by an unknown IRQ.
 *
 * If the current trap is a #GP caused by a non-present entry in the IDT, we
 * consider it an unknown interrupt which we acknowledge and ignore.
 *
 * Such unknown interrupts are usually caused by the firmware or the boot
 * loader configuring an interrupt source (e.g. the local APIC timer) and
 * failing to deactivate the source. The interrupt is then asserted while
 * interrupts are masked and there is no fail-safe way to deassert it.
 *
 * \param ts  Trap state.
 *
 * \retval 0  Unknown interrupt detected, the trap should be ignored.
 * \retval 1  The trap should be processed.
 */
PRIVATE inline
int
Thread::check_unknown_irq(Trap_state *ts)
{
  // Check for #GF and trap caused by invalid IDT entry (bit 1).
  if (ts->_trapno == 13 && (ts->_err & 2) == 2)
    {
      // Interrupt vector derived from the IDT selector index.
      Mword vector = (ts->_err & 0xffff) >> 3;

      // Ignore non-IRQ interrupt vectors.
      if (vector < 32 || vector >= Idt::_idt_max)
        return 1;

      if (!Idt::get(vector).present())
        {
          WARN("Unknown interrupt vector %lu, ignoring\n", vector);
          Apic::irq_ack();
          return 0;
        }
    }

  return 1;
}

PRIVATE inline NEEDS["keycodes.h"]
int
Thread::handle_not_nested_trap(Trap_state *ts)
{
  // no kernel debugger present
  printf(" %p IP=" L4_PTR_FMT " Trap=%02lx [Ret/Esc]\n",
         static_cast<void *>(this), ts->ip(), ts->_trapno);

  int r;
  // cannot use normal getchar because it may block with hlt and irq's
  // are off here
  while ((r = Kconsole::console()->getchar(false)) == -1)
    Proc::pause();

  if (r == KEY_ESC)
    terminate (1);

  return 0;
}

PROTECTED inline
int
Thread::sys_control_arch(Utcb const *, Utcb *)
{
  return 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && (debug || kdb) && !mp]:

PRIVATE static inline Cpu_number Thread::dbg_find_cpu() { return Cpu_number::boot_cpu(); }

//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && (debug || kdb) && mp]:

#include "apic.h"

PRIVATE static inline NEEDS["apic.h"]
Cpu_number
Thread::dbg_find_cpu()
{
  Apic_id phys_cpu = Apic::get_id();
  Cpu_number log_cpu = Apic::find_cpu(phys_cpu);
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%x\n", cxx::int_value<Apic_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }
  return log_cpu;
}


//---------------------------------------------------------------------------
IMPLEMENTATION [(ia32  || amd64) && !(debug || kdb)]:

/** There is no nested trap handler if both jdb and kdb are disabled.
 * Important: We don't need the nested_handler_stack here.
 */
PRIVATE static inline
int
Thread::call_nested_trap_handler(Trap_state *)
{ return -1; }

//----------------------------------------------------------------------------
IMPLEMENTATION [(ia32 || amd64) && virt_obj_space]:

IMPLEMENT_OVERRIDE inline
bool
Thread::pagein_tcb_request(Return_frame *regs)
{
  // Counterpart: Mem_layout::read_special_safe()
  unsigned long new_ip = regs->ip();
  if (*reinterpret_cast<Unsigned8*>(new_ip) == 0x48) // REX.W
    new_ip += 1;

  Unsigned16 op = *reinterpret_cast<Unsigned16*>(new_ip);
  if ((op & 0xc0ff) == 0x8b)
    {
      regs->ip(new_ip + 2);
      // stack layout:
      //         user eip
      //         PF error code
      // reg =>  eax/rax
      //         ecx/rcx
      //         edx/rdx
      //         ...
      Mword *reg = reinterpret_cast<Mword*>(regs) - 2
                   - Return_frame::Pf_ax_offset;
      assert((op >> 11) <= 2);
      reg[-(op>>11)] = 0; // op==0 => eax, op==1 => ecx, op==2 => edx

      // tell program that a pagefault occurred we cannot handle
      regs->flags(regs->flags() | 0x41); // set carry and zero flag in EFLAGS
      return true;
    }

  return false;
}
