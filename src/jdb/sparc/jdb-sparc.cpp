IMPLEMENTATION [sparc]:

#include "globals.h"
#include "kmem_alloc.h"
#include "space.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "static_init.h"


STATIC_INITIALIZE_P(Jdb, JDB_INIT_PRIO);

DEFINE_PER_CPU static Per_cpu<Proc::Status> jdb_irq_state;

// disable interrupts before entering the kernel debugger
IMPLEMENT
void
Jdb::save_disable_irqs(Cpu_number cpu)
{
  jdb_irq_state.cpu(cpu) = Proc::cli_save();
}

// restore interrupts after leaving the kernel debugger
IMPLEMENT
void
Jdb::restore_irqs(Cpu_number cpu)
{
  Proc::sti_restore(jdb_irq_state.cpu(cpu));
}

IMPLEMENT inline
void
Jdb::enter_trap_handler(Cpu_number)
{}

IMPLEMENT inline
void
Jdb::leave_trap_handler(Cpu_number)
{}

IMPLEMENT inline
bool
Jdb::handle_conditional_breakpoint(Cpu_number, Jdb_entry_frame *)
{ return false; }

IMPLEMENT
void
Jdb::handle_nested_trap(Jdb_entry_frame *e)
{
  printf("Trap in JDB: IP:%08lx SRR1=%08lx\n",
         e->ip(), e->srr1);
}

IMPLEMENT
bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  Jdb_entry_frame *ef = entry_frame.cpu(cpu);
  error_buffer.cpu(cpu).clear();
  // XXX kdb_ke() / kdb_ke_nstr()
  error_buffer.cpu(cpu).printf("%s", ef->text());

  return true;
}

IMPLEMENT inline
bool
Jdb::handle_user_request(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);

  if (ef->debug_ipi())
    return cpu != Cpu_number::boot_cpu();

  // XXX kdb_ke_sequence
  return false;
}

IMPLEMENT inline
bool
Jdb::test_checksums()
{ return true; }

static
bool
Jdb::handle_special_cmds(int)
{ return 1; }

PUBLIC static
FIASCO_INIT FIASCO_NOINLINE void
Jdb::init()
{
  static Jdb_handler enter(at_jdb_enter);
  static Jdb_handler leave(at_jdb_leave);

  Jdb::jdb_enter.add(&enter);
  Jdb::jdb_leave.add(&leave);

  Thread::nested_trap_handler = (Trap_state::Handler)enter_jdb;
  Kconsole::console()->register_console(push_cons());
}


PRIVATE static
unsigned char *
Jdb::access_mem_task(Jdb_address addr, bool write)
{
  // no need to special case write here, the kernel runs
  // on writable physical memory.
  (void) write;

  Address phys;

  if (addr.is_phys())
    phys = addr.phys();
  else if (addr.is_kmem())
    {
      phys = Kmem::virt_to_phys(addr.virt());
      if (phys == (Address)-1)
        return 0;

    }
  else
    {
      phys = Address(addr.space()->virt_to_phys(addr.addr()));

      if (phys == (Address)-1)
        phys = addr.space()->virt_to_phys_s0(addr.virt());

      if (phys == (Address)-1)
        return 0;
    }

   return (unsigned char*)phys;
}

PUBLIC static
int
Jdb::is_adapter_memory(Jdb_address)
{
  return 0;
}

PRIVATE static
void
Jdb::at_jdb_enter()
{
//  Mem_unit::clean_vdcache();
}

PRIVATE static
void
Jdb::at_jdb_leave()
{
//  Mem_unit::flush_vcache();
}

PUBLIC static inline
void
Jdb::enter_getchar()
{}

PUBLIC static inline
void
Jdb::leave_getchar()
{}

IMPLEMENT_OVERRIDE
void
Jdb::write_tsc_s(String_buffer *, Signed64 /*tsc*/, bool /*sign*/)
{}

IMPLEMENT_OVERRIDE
void
Jdb::write_tsc(String_buffer *, Signed64 /*tsc*/, bool /*sign*/)
{}

