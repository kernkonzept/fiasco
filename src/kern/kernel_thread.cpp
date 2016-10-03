INTERFACE:

#include "thread_object.h"

class Kernel_thread : public Thread_object
{
private:
  /**
   * Frees the memory of the initcall sections.
   *
   * Virtually initcall sections are freed by not marking them
   * reserved in the KIP. This method just invalidates the contents of
   * the memory, by filling it with some invalid data and may be
   * unmapping it.
   */
  void free_initcall_section();
  void bootstrap() asm ("call_bootstrap") FIASCO_FASTCALL;
  void bootstrap_arch();
  void run();
  void do_idle() __attribute__((noreturn));

protected:
  void init_workload();
};


IMPLEMENTATION:

#include <cstdlib>
#include <cstdio>

#include "config.h"
#include "cpu.h"
#include "delayloop.h"
#include "globals.h"
#include "helping_lock.h"
#include "kernel_task.h"
#include "per_cpu_data_alloc.h"
#include "processor.h"
#include "task.h"
#include "thread.h"
#include "thread_state.h"
#include "timer.h"
#include "timer_tick.h"
#include "watchdog.h"


PUBLIC explicit
Kernel_thread::Kernel_thread(Ram_quota *q)
: Thread_object(q, Thread::Kernel)
{}

PUBLIC inline
Mword *
Kernel_thread::init_stack()
{ return _kernel_sp; }

// the kernel bootstrap routine
IMPLEMENT FIASCO_INIT
void
Kernel_thread::bootstrap()
{
  // Initializations done -- Helping_lock can now use helping lock
  Helping_lock::threading_system_active = true;

  // we need per CPU data for our never running dummy CPU too
  // FIXME: we in fact need only the _pending_rqq lock
  Per_cpu_data_alloc::alloc(Cpu::invalid());
  Per_cpu_data::run_ctors(Cpu::invalid());
  set_current_cpu(Cpu::boot_cpu()->id());
  _home_cpu = Cpu::boot_cpu()->id();
  Mem::barrier();

  state_change_dirty(0, Thread_ready);		// Set myself ready

  Timer::init_system_clock();
  Sched_context::rq.current().set_idle(this->sched());

  Kernel_task::kernel_task()->make_current();

  // Setup initial timeslice
  Sched_context::rq.current().set_current_sched(sched());

  Timer_tick::setup(current_cpu());
  assert (current_cpu() == Cpu_number::boot_cpu()); // currently the boot cpu must be 0
  Mem_space::enable_tlb(current_cpu());

  Per_cpu_data::run_late_ctors(Cpu_number::boot_cpu());
  Per_cpu_data::run_late_ctors(Cpu::invalid());
  bootstrap_arch();

  Timer_tick::enable(current_cpu());
  Proc::sti();
  Watchdog::enable();
  printf("Calibrating timer loop... ");
  // Init delay loop, needs working timer interrupt
  Delay::init();
  printf("done.\n");

  run();
}

/**
 * The idle loop
 * NEVER inline this function, because our caller is an initcall
 */
IMPLEMENT FIASCO_NOINLINE FIASCO_NORETURN
void
Kernel_thread::run()
{
  free_initcall_section();

  // No initcalls after this point!

  kernel_context(home_cpu(), this);

  Rcu::leave_idle(home_cpu());
  // init_workload cannot be an initcall, because it fires up the userland
  // applications which then have access to initcall frames as per kinfo page.
  init_workload();

  for (;;)
    idle_op();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!arch_idle && !tickless_idle]:

PUBLIC inline NEEDS["processor.h"]
void
Kernel_thread::idle_op()
{
  if (Config::hlt_works_ok)
    Proc::halt();			// stop the CPU, waiting for an int
  else
    Proc::pause();
}


// ------------------------------------------------------------------------
IMPLEMENTATION [tickless_idle]:

#include <rcupdate.h>

EXTENSION class Kernel_thread
{
private:
  friend class Jdb_idle_stats;
  static Per_cpu<unsigned long> _idle_counter;
  static Per_cpu<unsigned long> _deep_idle_counter;
};


DEFINE_PER_CPU Per_cpu<unsigned long> Kernel_thread::_idle_counter;
DEFINE_PER_CPU Per_cpu<unsigned long> Kernel_thread::_deep_idle_counter;

// template code for arch idle
PUBLIC
void
Kernel_thread::idle_op()
{
  // this version must run with disabled IRQs and a wakup must continue directly
  // after the wait for event.
  auto guard = lock_guard(cpu_lock);
  Cpu_number cpu = home_cpu();
  ++_idle_counter.cpu(cpu);
  // 1. check for latency requirements that prevent low power modes
  // 2. check for timouts on this CPU ignore the idle thread's timeslice
  // 3. check for RCU work on this cpu
  if (Rcu::idle(cpu)
      && !Timeout_q::timeout_queue.cpu(cpu).have_timeouts(timeslice_timeout.cpu(cpu)))
    {
      ++_deep_idle_counter.cpu(cpu);
      Rcu::enter_idle(cpu);
      Timer_tick::disable(cpu);
      Mem_space::disable_tlb(cpu);
      Mem_unit::tlb_flush();

      // do everything to do to a deep sleep state:
      //  - flush caches
      //  - ...
      arch_tickless_idle(cpu);

      Mem_space::enable_tlb(cpu);
      Rcu::leave_idle(cpu);
      Timer_tick::enable(cpu);
    }
  else
    arch_idle(cpu);
}


