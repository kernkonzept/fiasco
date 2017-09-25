INTERFACE [sparc && debug]:

#include "trap_state.h"
#include "util.h"

EXTENSION class Thread
{
public:
  typedef void (*Dbg_extension_entry)(Thread *t, Entry_frame *r);
  static Dbg_extension_entry dbg_extension[64];

protected:
  static int call_nested_trap_handler(Trap_state *ts) asm ("call_nested_trap_handler");
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};

//------------------------------------------------------------------------------
IMPLEMENTATION [sparc && debug]:

Thread::Dbg_extension_entry Thread::dbg_extension[64];
Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

extern "C" void sys_kdb_ke()
{
  cpu_lock.lock();
  Thread *t = current_thread();

  //arriving from outx function
  //unsigned *x = (unsigned *)t->regs()->ip();
  if(t->regs()->r[29] == (Mword)-0x24 && t->regs()->r[2] & (1 << 31)) //ip && r3
    {
      NOT_IMPL_PANIC;
      unsigned func = (unsigned)t->regs()->r[1] & 0xffff; //r4
      if(Thread::dbg_extension[func])
        {
          Thread::dbg_extension[func](t, t->regs());
          return;
        }
    }

  char str[32] = "USER ENTRY";
  //arriving from enter_kdebug
  if (t->regs()->r[29] == (Mword)-0x24 && t->regs()->r[2] & (1 << 30)) //ip && r3
    {
      //t->mem_space()->copy_from_user(str, (char *)(x + 1), sizeof(str));
      str[sizeof(str)-1] = 0;
    }

  kdb_ke(str);
}

IMPLEMENT
int
Thread::call_nested_trap_handler(Trap_state * /*ts*/)
{
  Cpu_phys_id phys_cpu = Proc::cpu_id();
  Cpu_number log_cpu = Cpu::cpus.find_cpu(Cpu::By_phys_id(phys_cpu));
  if (log_cpu == Cpu_number::nil())
    {
      printf("Trap on unknown CPU phys_id=%x\n", cxx::int_value<Cpu_phys_id>(phys_cpu));
      log_cpu = Cpu_number::boot_cpu();
    }

  unsigned long &ntr = nested_trap_recover.cpu(log_cpu);

  void *stack = 0;

  if (!ntr)
    stack = dbg_stack.cpu(log_cpu).stack_top;

  return 0;
}

IMPLEMENTATION [sparc && !debug]:

extern "C" void sys_kdb_ke()
{
}
