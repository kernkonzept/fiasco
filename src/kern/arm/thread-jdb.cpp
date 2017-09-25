INTERFACE [arm-debug]:

#include "trap_state.h"

EXTENSION class Thread
{
public:
  typedef void (*Dbg_extension_entry)(Thread *t, Entry_frame *r);
  static Dbg_extension_entry dbg_extension[64];

protected:
  static int call_nested_trap_handler(Trap_state *ts) asm ("call_nested_trap_handler");
  static Trap_state::Handler nested_trap_handler FIASCO_FASTCALL;
};


//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && debug]:

#include "kernel_task.h"
#include "mem_layout.h"
#include "mmu.h"

#include <cstring>

Thread::Dbg_extension_entry Thread::dbg_extension[64];

Trap_state::Handler Thread::nested_trap_handler FIASCO_FASTCALL;

extern "C" void sys_kdb_ke()
{
  cpu_lock.lock();
  Thread *t = current_thread();
  Unsigned32 x = Thread::peek_user((Unsigned32 *)t->regs()->ip(), t);

  if (EXPECT_FALSE(t->is_kernel_mem_op_hit_and_clear()))
    x = 0;

  if ((x & 0xffff0000) == 0xe35e0000)
    {
      unsigned func = x & 0x3f;
      if (Thread::dbg_extension[func])
	{
	  Thread::dbg_extension[func](t, t->regs());
	  t->regs()->ip(t->regs()->ip() + 4);
	  return;
	}
    }

  char str[32] = "USER ENTRY";
  if ((x & 0xfffffff0) == 0xea000000)
    // check for always branch, no return, maximum 32 bytes forward
    {
      char const *user_str = reinterpret_cast<char const *>(t->regs()->ip() + 4);
      for (unsigned i = 0; i < sizeof(str); ++i)
        {
          str[i] = Thread::peek_user(user_str + i, t);
          if (EXPECT_FALSE(t->is_kernel_mem_op_hit_and_clear()))
            {
              str[0] = 0;
              break;
            }
          if (str[i] == 0)
            break;
        }

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

  Mem_space *m = Mem_space::current_mem_space(log_cpu);

  if (Kernel_task::kernel_task() != m)
    Kernel_task::kernel_task()->make_current();

  Mword dummy1, tmp, ret;
  {
    register Mword _ts asm("r0") = (Mword)ts;
    register Cpu_number _lcpu asm("r1") = log_cpu;

    asm volatile(
	"mov    %[origstack], sp	 \n"
	"ldr    %[tmp], [%[ntr]]         \n"
	"teq    %[tmp], #0               \n"
	"moveq  sp, %[stack]             \n"
	"add    %[tmp], %[tmp], #1       \n"
	"str    %[tmp], [%[ntr]]         \n"
	"str    %[origstack], [sp, #-4]! \n"
	"str    %[ntr], [sp, #-4]!       \n"
	"adr    lr, 1f                   \n"
	"mov    pc, %[handler]           \n"
	"1:                              \n"
	"ldr    %[ntr], [sp], #4         \n"
	"ldr	sp, [sp]                 \n"
	"ldr    %[tmp], [%[ntr]]         \n"
	"sub    %[tmp], %[tmp], #1       \n"
	"str    %[tmp], [%[ntr]]         \n"
	: [origstack] "=&r" (dummy1), [tmp] "=&r" (tmp),
	  "=r" (_ts), "=r" (_lcpu)
	: [ntr] "r" (&ntr), [stack] "r" (stack),
	  [handler] "r" (*nested_trap_handler),
	  "2" (_ts), "3" (_lcpu)
	: "memory", "r2", "r3", "r4");

    ret = _ts;
  }

  // the jdb-cpu might have changed things we shouldn't miss!
  Mmu<Mem_layout::Cache_flush_area, true>::flush_cache();
  Mem::isb();

  if (m != Kernel_task::kernel_task())
    m->make_current();

  if (!ntr)
    Cpu_call::handle_global_requests();

  return ret;
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm-!debug]:

extern "C" void sys_kdb_ke()
{}

PRIVATE static inline
int
Thread::call_nested_trap_handler(Trap_state *)
{ return -1; }
