INTERFACE [riscv]:

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
  static int call_nested_trap_handler(Trap_state *ts);
};

//----------------------------------------------------------------------------
INTERFACE [riscv && debug]:

#include "trap_state.h"

EXTENSION class Thread
{
protected:
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};

//-----------------------------------------------------------------------------
IMPLEMENTATION [riscv && debug]:

#include "asm_riscv.h"
#include "cpu.h"
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
  void *stack = !ntr ? dbg_stack.cpu(log_cpu).stack_top : nullptr;

  Mem_space *m = Mem_space::current_mem_space(log_cpu);

  if (Kernel_task::kernel_task() != m)
    Kernel_task::kernel_task()->make_current();

  if (EXPECT_FALSE(!nested_trap_handler))
    {
      ts->dump();
      panic("Nested trap handler not yet initialized");
    }

  int ret = riscv_enter_debugger(ts, log_cpu, &ntr, stack);

  if (m != Kernel_task::kernel_task())
    m->make_current();

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}

PRIVATE static inline NEEDS["cpu.h"]
int
Thread::riscv_enter_debugger(Trap_state *ts, Cpu_number log_cpu,
                           unsigned long *ntr, void *stack)
{
  // Holds first argument and afterwards the return value.
  register Mword _ts asm("a0") = reinterpret_cast<Mword>(ts);
  register Cpu_number _log_cpu asm("a1") = log_cpu;

  register unsigned long *_ntr asm("t0") = ntr;
  register void *_stack asm("t1") = stack;
  register Trap_state::Handler _handler asm("t2") = nested_trap_handler;

  __asm__ __volatile__(
    // Save original stack pointer
    "mv t4, sp                    \n"

    // Switch to debug stack if not inside a trap within a trap handler.
    REG_L " t5, (%[ntr])          \n"
    "bnez t5, 1f                  \n"
    "mv sp, %[stack]              \n"
    "1:                           \n"

    // Increment nested trap recover
    "addi t5, t5, 1               \n"
    REG_S " t5, (%[ntr])          \n"

    // Preserve ntr and original stack pointer
    "addi sp, sp, -%[stack_frame] \n"
    REG_S " %[ntr], 0(sp)         \n"
    REG_S " t4, %[reg_size](sp)   \n"

    "jalr %[handler]              \n"

    // Restore ntr and original stack pointer
    REG_L " %[ntr], 0(sp)         \n"
    REG_L " t4, %[reg_size](sp)   \n"
    "addi sp, sp, %[stack_frame]  \n"

    // Decrement nested trap recover
    REG_L " t5, (%[ntr])          \n"
    "addi t5, t5, -1              \n"
    REG_S " t5, (%[ntr])          \n"

    // Restore original stack pointer
    "mv sp, t4                    \n"
    :
      "+r" (_ts), "+r" (_log_cpu),
      [ntr] "+r" (_ntr), [stack] "+r" (_stack), [handler] "+r" (_handler)
    :
      [stack_frame] "n" (Cpu::stack_round(sizeof(Mword) * 2)),
      [reg_size] "n" (sizeof(Mword))
    :
      /* "a0", "a1", */ "a2", "a3", "a4", "a5", "a6", "a7",
      /* "t0", "t1", "t2", */ "t3", "t4", "t5", "t6",
      "ra", "gp", "tp", "memory"
  );

  return _ts;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [riscv && !debug]:

IMPLEMENT
int
Thread::call_nested_trap_handler(Trap_state *ts)
{
  ts->dump();
  current_thread()->kill();
  return -1;
}
