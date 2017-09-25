INTERFACE [ux]:

#include <sys/types.h>

class Trap_state;

EXTENSION class Thread
{
private:
  static int (*int3_handler)(Trap_state*);
};

IMPLEMENTATION [ux]:

#include <cstdio>
#include <csignal>
#include "boot_info.h"
#include "config.h"		// for page sizes
#include "emulation.h"
#include "lock_guard.h"
#include "panic.h"
#include "per_cpu_data.h"
#include "utcb_init.h"

int (*Thread::int3_handler)(Trap_state*);
DEFINE_PER_CPU Per_cpu<Thread::Dbg_stack> Thread::dbg_stack;


IMPLEMENT static inline NEEDS ["emulation.h"]
Mword
Thread::exception_cs()
{
  return Emulation::kernel_cs();
}

/**
 * The ux specific part of the thread constructor.
 */
PRIVATE inline
void
Thread::arch_init()
{
  // Allocate FPU state now because it indirectly calls current()
  // save_state runs on a signal stack and current() doesn't work there.
  if (space())
    // FIXME, space()==0 for sigma0 (or even more)
    Fpu_alloc::alloc_state(space()->ram_quota(), fpu_state());
  else
    Fpu_alloc::alloc_state(Ram_quota::root, fpu_state());

  // clear out user regs that can be returned from the thread_ex_regs
  // system call to prevent covert channel
  Entry_frame *r = regs();
  r->sp(0);
  r->ip(0);
  r->cs(Emulation::kernel_cs() & ~1);			// force iret trap
  r->ss(Emulation::kernel_ss());
  r->flags(EFLAGS_IOPL_K | EFLAGS_IF | 2);
}

PUBLIC static inline
void
Thread::set_int3_handler(int (*handler)(Trap_state *ts))
{
  int3_handler = handler;
}

PRIVATE inline bool Thread::check_trap13_kernel(Trap_state *)
{ return 1; }

PRIVATE inline void Thread::check_f00f_bug(Trap_state *)
{}

PRIVATE inline 
unsigned
Thread::check_io_bitmap_delimiter_fault(Trap_state *)
{ return 1; }

PRIVATE inline bool Thread::handle_sysenter_trap(Trap_state *, Address, bool)
{ return true; }

PRIVATE inline
int
Thread::handle_not_nested_trap(Trap_state *)
{ return -1; }

PRIVATE
int
Thread::call_nested_trap_handler(Trap_state *ts)
{
  // run the nested trap handler on a separate stack
  // equiv of: return nested_trap_handler(ts) == 0 ? true : false;

  //static char nested_handler_stack [Config::PAGE_SIZE];
  Cpu_phys_id phys_cpu = Cpu::phys_id_direct();
  Cpu_number log_cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(phys_cpu));

  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU host-thread=%x\n",
             cxx::int_value<Cpu_phys_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }

  unsigned long &ntr = nested_trap_recover.cpu(log_cpu);

  //printf("%s: lcpu%u sp=%p t=%lu nested_trap_recover=%ld handler=%p\n",
  //    __func__, log_cpu, (void*)Proc::stack_pointer(), ts->_trapno,
  //    ntr, nested_trap_handler);

  int ret;
  void *stack = 0;

  if (!ntr)
    stack = dbg_stack.cpu(log_cpu).stack_top;

  unsigned dummy1, dummy2, dummy3;

  asm volatile
    ("movl   %%esp,%[d2]	\n\t"
     "cmpl   $0, (%[ntrp])      \n\t"
     "jne    1f                 \n\t"
     "movl   %[stack],%%esp	\n\t"
     "1:			\n\t"
     "incl   (%[ntrp])          \n\t"
     "pushl  %[d2]              \n\t"
     "pushl  %[ntrp]            \n\t"
     "call   *%[handler]        \n\t"
     "popl   %[ntrp]            \n\t"
     "popl   %%esp              \n\t"
     "cmpl   $0,(%[ntrp])       \n\t"
     "je     1f                 \n\t"
     "decl   (%[ntrp])          \n\t"
     "1:                        \n\t"
     : [ret] "=a" (ret),
       [d1]  "=&c" (dummy1),
       [d2]  "=&r" (dummy2),
             "=d"  (dummy3)
     : [ts]      "a"(ts),
       [cpu]     "d" (log_cpu),
       [ntrp]    "r" (&ntr),
       [stack]   "r" (stack),
       [handler] "m" (nested_trap_handler)
     : "memory");

  assert (_magic == magic);

  return ret == 0 ? 0 : -1;
}

// The "FPU not available" trap entry point
extern "C" void thread_handle_fputrap (void) { panic ("fpu trap"); }

extern "C" void thread_timer_interrupt_slow() {}

IMPLEMENT
void
Thread::user_invoke()
{
    {
      Mword dummy;
      asm volatile
        ("  movl %%ds ,  %0         \n\t"
         "  movl %0,     %%es	\n\t"
         : "=r"(dummy));
    }

  Cpu::set_gs(Utcb_init::utcb_segment());
  Cpu::set_fs(Utcb_init::utcb_segment());

  user_invoke_generic();

  asm volatile
    ("  movl %%eax , %%esp      \n\t"
     "  xorl %%ebx , %%ebx      \n\t"
     "  xorl %%edx , %%edx      \n\t"
     "  xorl %%esi , %%esi      \n\t"     // clean out user regs
     "  xorl %%edi , %%edi      \n\t"
     "  xorl %%ebp , %%ebp      \n\t"
     "  xorl %%eax , %%eax      \n\t"
     "  iret                    \n\t"
     : // no output
     : "a" (nonull_static_cast<Return_frame*>(current()->regs())),
       "c" (current()->space()->is_sigma0() // only Sigma0 gets the KIP
            ? Kmem::virt_to_phys(Kip::k()) : 0));
}

PROTECTED inline
int
Thread::sys_control_arch(Utcb *utcb)
{
  if (utcb->values[0] & Ctl_ux_native)
    _is_native = utcb->values[4] & Ctl_ux_native;
  return 0;
}

//---------------------------------------------------------------------------
IMPLEMENTATION [ux]:

#include "gdt.h"
#include <feature.h>

KIP_KERNEL_FEATURE("segments");

PROTECTED inline
L4_msg_tag
Thread::invoke_arch(L4_msg_tag tag, Utcb *utcb)
{
  switch (utcb->values[0] & Opcode_mask)
    {
      case Op_gdt_x86: // Gdt operation

      // if no words given then return the first gdt entry
      if (tag.words() == 1)
        {
          utcb->values[0] = Emulation::host_tls_base();
          return commit_result(0, 1);
        }

        {
          unsigned entry_number = utcb->values[1];
          unsigned idx = 2;
          Mword  *trampoline_page = (Mword *) Kmem::phys_to_virt
                                              (Mem_layout::Trampoline_frame);


          for (; entry_number < Gdt_user_entries
                 && idx < tag.words()
               ; idx += 2, ++entry_number)
            {
              Gdt_entry *d = (Gdt_entry *)&utcb->values[idx];
              if (!d->limit())
                continue;

              Ldt_user_desc info;
	      info.entry_number    =  entry_number + Emulation::host_tls_base();
	      info.base_addr       =  d->base();
	      info.limit           =  d->limit();
	      info.seg_32bit       =  d->seg32();
	      info.contents        =  d->contents();
	      info.read_exec_only  = !d->writable();
	      info.limit_in_pages  =  d->granularity();
	      info.seg_not_present = !d->present();
	      info.useable         =  d->avl();

	      // Remember descriptor for reload on thread switch
	      memcpy(&_gdt_user_entries[entry_number], &info,
                     sizeof(_gdt_user_entries[0]));

	      // Set up data on trampoline
	      memcpy(trampoline_page + 1, &info, sizeof(info));

	      // Call set_thread_area for given user process
	      Trampoline::syscall(space()->pid(), 243 /* __NR_set_thread_area */,
                                  Mem_layout::Trampoline_page + sizeof(Mword));

	      // Also set this for the fiasco kernel so that
	      // segment registers can be set, this is necessary for signal
	      // handling, esp. for sigreturn to work in the Fiasco kernel
	      // with the context of the client (gs/fs values).
	      Emulation::thread_area_host(entry_number + Emulation::host_tls_base());
            }

          if (this == current_thread())
            load_gdt_user_entries();

          return commit_result(((utcb->values[1] + Emulation::host_tls_base()) << 3) + 3);
        }

    default:
      return commit_result(-L4_err::ENosys);
    };
}
