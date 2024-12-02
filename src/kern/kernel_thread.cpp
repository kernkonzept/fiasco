INTERFACE:

#include "thread_object.h"

class Kernel_thread : public Thread_object
{
public:
  /**
   * Desired UTCB address for sigma0 and boot tasks.
   */
  static Address utcb_addr();

protected:
  /**
   * Idle operation the kernel idle threads execute in a loop on each CPU.
   */
  void idle_op();

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
  [[noreturn]] void bootstrap() asm ("call_bootstrap") FIASCO_FASTCALL;
  void bootstrap_arch();
  [[noreturn]] void run();
  void check_debug_koptions();
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
#include "kmem.h"
#include "mem_layout.h"
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

IMPLEMENT_DEFAULT inline NEEDS["mem_layout.h"]
Address
Kernel_thread::utcb_addr()
{ return Mem_layout::Utcb_addr; }

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
  Kmem::kernel_remap();
  bootstrap_arch();

  // Needs to be done before the timer is enabled. Otherwise after returning
  // from printf() there could be a burst of timer interrupts distorting the
  // timer loop calibration. The measurement intervals would be far too short.
  printf("Calibrating timer loop...\n");
  Timer_tick::enable(current_cpu());
  Proc::sti();
  Watchdog::enable();
  // Init delay loop, needs working timer interrupt
  Delay::init();
  printf("Timer calibration done.\n");

  run();
}

/**
 * The idle loop
 * NEVER inline this function, because our caller is an initcall
 */
IMPLEMENT FIASCO_NOINLINE
void
Kernel_thread::run()
{
  free_initcall_section();

  // No initcalls after this point!

  kernel_context(home_cpu(), this);

  Rcu::leave_idle(home_cpu());

  check_debug_koptions();

  init_workload();

  for (;;)
    idle_op();
}

IMPLEMENT_DEFAULT
void
Kernel_thread::check_debug_koptions()
{}

PRIVATE
void
Kernel_thread::init_workload()
{
  extern void (*_init_workload_table[])();
  for (unsigned i = 0; _init_workload_table[i]; ++i)
    _init_workload_table[i]();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [debug]:

#include "koptions.h"
#include "string_buffer.h"

IMPLEMENT_OVERRIDE
void
Kernel_thread::check_debug_koptions()
{
  auto g = lock_guard(cpu_lock);

  if constexpr (Config::Jdb)
    {
      if (   !Koptions::o()->opt(Koptions::F_nojdb)
          && Koptions::o()->opt(Koptions::F_jdb_cmd))
        {
          // extract the control sequence from the command line
          String_buf<128> cmd;
          for (char const *s = Koptions::o()->jdb_cmd; *s && *s != ' '; ++s)
            cmd.append(*s);

          kdb_ke_sequence(cmd.c_str(), cmd.length());
        }
    }

  // kernel debugger rendezvous
  if (Koptions::o()->opt(Koptions::F_wait))
    kdb_ke("Wait");
}

// ------------------------------------------------------------------------
IMPLEMENTATION [!tickless_idle]:

#include "processor.h"

IMPLEMENT_DEFAULT inline NEEDS["processor.h"]
void
Kernel_thread::idle_op()
{
  Proc::halt(); // stop the CPU, waiting for an interrupt
}

// ------------------------------------------------------------------------
IMPLEMENTATION [tickless_idle]:

#include <rcupdate.h>
#include <tlbs.h>

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
IMPLEMENT
void
Kernel_thread::idle_op()
{
  // this version must run with disabled IRQs and a wakeup must continue
  // directly after the wait for event.
  auto guard = lock_guard(cpu_lock);
  Cpu_number cpu = home_cpu();
  ++_idle_counter.cpu(cpu);
  // 1. check for latency requirements that prevent low power modes
  // 2. check for timeouts on this CPU ignore the idle thread's timeslice
  // 3. check for RCU work on this CPU
  if (Rcu::idle(cpu)
      && !Timeout_q::timeout_queue.cpu(cpu).have_timeouts(timeslice_timeout.cpu(cpu)))
    {
      ++_deep_idle_counter.cpu(cpu);
      Rcu::enter_idle(cpu);
      Timer_tick::disable(cpu);
      Mem_space::disable_tlb(cpu);
      Tlb::flush_all_cpu(cpu);

      // do everything to do to a deep sleep state:
      //  - flush caches
      //  - ...
      arch_tickless_idle(cpu);

      Mem_space::enable_tlb(cpu);
      Mem_space::reload_current();
      Rcu::leave_idle(cpu);
      Timer_tick::enable(cpu);
    }
  else
    arch_idle(cpu);
}


