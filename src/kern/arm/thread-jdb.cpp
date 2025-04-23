INTERFACE [arm]:

EXTENSION class Thread
{
protected:
  /**
   * Call a trap handler supposed to enter a debugger.
   * Use a separate stack (per-CPU dbg_stack).
   *
   * \param ts  Trap state.
   * \retval 0 trap has been consumed by the handler.
   * \retval -1 trap could not be handled.
   */
  static int call_nested_trap_handler(Trap_state *ts) asm ("call_nested_trap_handler");
  static int arm_serror_handler(Trap_state *ts) asm ("arm_serror_handler");
};

INTERFACE [arm-debug]:

#include "trap_state.h"

EXTENSION class Thread
{
protected:
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && debug && (64bit || cpu_virt)]:

PRIVATE static inline NOEXPORT bool
Thread::debugger_needs_switch_to_kdir() { return false; }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && debug && !(64bit || cpu_virt)]:

PRIVATE static inline NOEXPORT bool
Thread::debugger_needs_switch_to_kdir() { return true; }

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && debug]:

#include "kernel_task.h"
#include "mem_layout.h"
#include "mmu.h"

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

IMPLEMENT
int
Thread::call_nested_trap_handler(Trap_state *ts)
{
  Cpu_phys_id phys_cpu = Proc::cpu_id();
  Cpu_number log_cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(phys_cpu));
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%x\n",
             cxx::int_value<Cpu_phys_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }

  unsigned long &ntr = nested_trap_recover.cpu(log_cpu);

  void *stack = nullptr;

  if (!ntr)
    stack = dbg_stack.cpu(log_cpu).stack_top;

  Mem_space *m = Mem_space::current_mem_space(log_cpu);

  if (debugger_needs_switch_to_kdir() && (Kernel_task::kernel_task() != m))
    Kernel_task::kernel_task()->make_current();

  if (EXPECT_FALSE(!nested_trap_handler))
    {
      ts->dump();
      panic("Nested trap handler not yet initialized");
    }

  int ret = arm_enter_debugger(ts, log_cpu, &ntr, stack);

  // the jdb-cpu might have changed things we shouldn't miss!
  Mmu::flush_cache();
  Mem::isb();

  if (debugger_needs_switch_to_kdir() && (m != Kernel_task::kernel_task()))
    m->make_current();

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "globals.h"

IMPLEMENT
int
Thread::arm_serror_handler(Trap_state *)
{
  ++(num_serrors.cpu(current_cpu()));
  return 0;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm-!debug]:

IMPLEMENT
int
Thread::call_nested_trap_handler(Trap_state *ts)
{
  ts->dump();
  current_thread()->kill();
  return -1;
}
