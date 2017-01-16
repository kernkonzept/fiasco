INTERFACE [arm-debug]:

#include "trap_state.h"

EXTENSION class Thread
{
protected:
  static int call_nested_trap_handler(Trap_state *ts) asm ("call_nested_trap_handler");
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

  void *stack = 0;

  if (!ntr)
    stack = dbg_stack.cpu(log_cpu).stack_top;

  Mem_space *m = Mem_space::current_mem_space(log_cpu);

  if (debugger_needs_switch_to_kdir() && (Kernel_task::kernel_task() != m))
    Kernel_task::kernel_task()->make_current();

  int ret = arm_enter_debugger(ts, log_cpu, &ntr, stack);

  // the jdb-cpu might have changed things we shouldn't miss!
  Mmu<Mem_layout::Cache_flush_area, true>::flush_cache();
  Mem::isb();

  if (debugger_needs_switch_to_kdir() && (m != Kernel_task::kernel_task()))
    m->make_current();

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm-!debug]:

PRIVATE static inline
int
Thread::call_nested_trap_handler(Trap_state *)
{ return -1; }
