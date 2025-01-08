/*
 * Copyright (C) 2014 Imagination Technologies Ltd.
 * Author: Sanjay Lal <sanjayl@kymasys.com>
 * Author: Yann Le Du <ledu@kymasys.com>
 * Author: Alexander Warg <alexander.warg@kernkonzept.com>
 */

INTERFACE [mips && debug]:

#include "trap_state.h"

EXTENSION class Thread
{
public:
  typedef void (*Dbg_extension_entry)(Thread *t, Trap_state *ts);
  static Dbg_extension_entry dbg_extension[64];

  /**
   * Call a trap handler supposed to enter a debugger.
   * Use a separate stack (per-CPU dbg_stack).
   *
   * \param ts  Trap state.
   * \retval 0 trap has been consumed by the handler.
   * \retval -1 trap could not be handled.
   */
  static int call_nested_trap_handler(Trap_state *ts);

  enum class Kernel_entry_op : Mword
  {
    Kdebug_none = 0,
    Kdebug_text = 1,
    Kdebug_call = 2,
  };

private:
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};

//------------------------------------------------------------------------------
IMPLEMENTATION [mips && debug]:

#include "kernel_task.h"
#include "mem_layout.h"
#include "thread.h"

#include <cstring>

Thread::Dbg_extension_entry Thread::dbg_extension[64];
Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

extern "C" void sys_kdb_ke()
{
  cpu_lock.lock();
  char str[32] = "BREAK ENTRY";
  Thread *t = current_thread();
  Entry_frame *regs = t->regs();
  Trap_state *ts = static_cast<Trap_state*>(regs);
  Mword arg = ts->r[Trap_state::R_t0];

  switch (Thread::Kernel_entry_op{ts->r[Trap_state::R_s6]})
    {
    case Thread::Kernel_entry_op::Kdebug_call:
      if (arg > sizeof(Thread::dbg_extension)/sizeof(Thread::dbg_extension[0]))
        break;

      if (!Thread::dbg_extension[arg])
        break;

      Thread::dbg_extension[arg](t, ts);
      return;

    case Thread::Kernel_entry_op::Kdebug_text:
      {
        auto *arg_str = reinterpret_cast<char const *>(arg);
        size_t len = min(strlen(arg_str), sizeof(str) - 1);
        memcpy(str, arg_str, len);
        str[len] = '\0';
        break;
      }

    default:
      break;
    }

  kdb_ke(str);
}

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

#if 0
  Mem_space *m = Mem_space::current_mem_space(log_cpu);
  if (Kernel_task::kernel_task() != m)
    Kernel_task::kernel_task()->make_current();
#endif
  // FIXME: AW what does this do
  // ts->set_errorcode(ts->error() | ts->r[Syscall_frame::REG_T0]);

  Mword dummy1, tmp, ret;
  {
    register Mword _ts asm("$4") = reinterpret_cast<Mword>(ts); // $4 == a0
    register Mword res asm("$2");                               // $2 == v0
    register Cpu_number _lcpu asm("$5") = log_cpu;              // $5 == a1

    asm volatile(
        ".set push                        \n"
        ".set noreorder                   \n"
        " move  %[origstack], $sp         \n"
        " " ASM_L " %[tmp], 0(%[ntr])     \n"
        " bnez  %[tmp], 1f                \n"
        " nop                             \n"
        " move  $sp, %[stack]             \n"
        "1:                               \n"
        " " ASM_ADDIU " %[tmp], %[tmp], 1 \n"
        " " ASM_S " %[tmp], 0(%[ntr])     \n"
        " " ASM_ADDIU " $sp, $sp, -(%[frsz] + 2 * %[rsz]) \n" //set up call frame
        " " ASM_S " %[origstack], (%[rsz] + %[frsz])($sp) \n"
        " " ASM_S " %[ntr], (%[frsz])($sp)                \n"
        " jalr  %[handler]                \n"
        " nop                             \n"
        " " ASM_L " %[ntr], (%[frsz])($sp)\n"
        " " ASM_L " $sp, (%[rsz] + %[frsz])($sp)          \n"
        " " ASM_L " %[tmp], 0(%[ntr])     \n"
        " " ASM_ADDIU " %[tmp], %[tmp], -1\n"
        " " ASM_S " %[tmp], 0(%[ntr])     \n"
        ".set pop                         \n"
        : [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
          "+r" (_ts), "+r" (_lcpu), "+r" (res)
        : [ntr] "r" (&ntr), [stack] "r" (stack),
          [handler] "r" (nested_trap_handler),
          [rsz] "n" (sizeof(Mword)),
          [frsz] "n" (ASM_NARGSAVE * sizeof(Mword))
        : "memory", "$3", "$6", "$7", "$8", "$9", "$10", "$11",
           "$12", "$13", "$14", "$15", "$24", "$25", "$31");

    ret = res;
    ts->epc +=4;
  }

  // the jdb-cpu might have changed things we shouldn't miss!
  // FIXME: MIPS CACHE
  //Mmu<Mem_layout::Cache_flush_area, true>::flush_cache();
#if 0
  if (m != Kernel_task::kernel_task())
    m->make_current();
#endif
  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}

IMPLEMENTATION [mips && !debug]:

extern "C" void sys_kdb_ke()
{}

PUBLIC static inline
int
Thread::call_nested_trap_handler(Trap_state *)
{ return -1; }
