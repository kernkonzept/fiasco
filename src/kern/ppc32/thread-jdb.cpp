INTERFACE [ppc32 && debug]:

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
IMPLEMENTATION [ppc32 && debug]:

Thread::Dbg_extension_entry Thread::dbg_extension[64];
Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

extern "C" void sys_kdb_ke()
{
  cpu_lock.lock();
  Thread *t = current_thread();

  //arriving from outx function
  unsigned *x = (unsigned *)t->regs()->ip();
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
      strncpy(str, (char *)(x + 1), sizeof(str));
      str[sizeof(str)-1] = 0;
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

  Mword dummy1, tmp;
  {
    register Mword _ts asm("3") = (Mword)ts;
    register Cpu_number _lcpu asm("r4") = log_cpu;

    asm volatile(
	" mr    %[origstack], %%r1        \n"
	" lwz   %[tmp], 0(%[ntr])         \n"
	" cmpwi %[tmp], 0                 \n"
	" bne 1f                          \n"
	" mr %%r1, %[stack]               \n"
	"1:                               \n"
	" mtsprg1 %%r1                    \n"
	" addi  %[tmp], %[tmp], 1         \n"
	" stw   %[tmp], 0(%[ntr])         \n"
	" subi  %%r1, %%r1, 16            \n" //extent stack frame
	" stw   %[origstack], 16(%%r1)    \n"
	" stw   %[ntr],       12(%%r1)    \n"
	" lis   %[tmp], 2f@ha             \n"
	" addi  %[tmp], %[tmp], 2f@l      \n"
	" mtlr  %[tmp]                    \n"
	" mtctr %[handler]                \n"
	" bctr                            \n"
	"2:                               \n"
	" lwz   %[ntr], 12(%%r1)          \n"
	" lwz   %%r1,   16(%%r1)          \n"
	" lwz   %[tmp],  0(%[ntr])        \n"
	" subi  %[tmp], %[tmp], 1         \n"
	" stw   %[tmp],  0(%[ntr])        \n"
	: [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
	  "=r" (_ts), "=r" (_lcpu)
	: [ntr] "r" (&ntr), [stack] "r" (stack),
	  [handler] "r" (*nested_trap_handler),
	  "2" (_ts), "3" (_lcpu)
	: "memory");

    return _ts;
  }
}

IMPLEMENTATION [ppc32 && !debug]:

extern "C" void sys_kdb_ke()
{
}
