IMPLEMENTATION [mips]:

#include "globals.h"
#include "kmem_alloc.h"
#include "space.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "static_init.h"


STATIC_INITIALIZE_P(Jdb, JDB_INIT_PRIO);

// disable interrupts before entering the kernel debugger
IMPLEMENT
void
Jdb::save_disable_irqs(Cpu_number)
{}

// restore interrupts after leaving the kernel debugger
IMPLEMENT
void
Jdb::restore_irqs(Cpu_number cpu)
{
  Ipi::atomic_reset(cpu, Ipi::Debug);
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
  printf("Trap in JDB: IP:%08lx Cause=%08lx Status=%08lx\n",
         e->ip(), e->cause, e->status);
}

IMPLEMENT
bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);
  error_buffer.cpu(cpu).clear();

  if (ef->debug_ipi())
    error_buffer.cpu(cpu).printf("IPI ENTRY");
  else if (ef->debug_entry_kernel_str())
    error_buffer.cpu(cpu).printf("%s", ef->text());
  else if (ef->debug_entry_user_str())
    error_buffer.cpu(cpu).printf("user: \"%.*s\"", ef->textlen(), ef->text());
  else
    error_buffer.cpu(cpu).printf("ENTRY");

  return true;
}

IMPLEMENT inline
bool
Jdb::handle_user_request(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);

  if (ef->debug_ipi())
    return cpu != Cpu_number::boot_cpu();

  if (ef->debug_entry_kernel_sequence())
    return execute_command_ni(ef->text(), ef->textlen());

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
  Thread::nested_trap_handler = (Trap_state::Handler)enter_jdb;
  Kconsole::console()->register_console(push_cons());
}


PRIVATE static
unsigned char *
Jdb::access_mem_task(Jdb_address addr, bool write)
{
  // no special need for MIPS here all returned mappings are writable
  (void) write;

  Address phys;
  if (addr.is_phys())
    phys = addr.phys();
  else if (addr.is_kmem())
    {
      if (addr.addr() >= Mem_layout::KSEG0 && addr.addr() <= Mem_layout::KSEG0e)
        return (unsigned char *)addr.virt();

      phys = addr.addr();
    }
  else
    {
      phys = addr.space()->virt_to_phys_s0(addr.virt());

      if (phys == (Address)-1)
        return 0;
    }

  // physical memory accessible via unmapped KSEG0
  if (phys <= Mem_layout::KSEG0e - Mem_layout::KSEG0)
    return (unsigned char *)(phys + Mem_layout::KSEG0);

  // FIXME: temp mapping for the physical memory needed
  return 0;
}

PUBLIC static
int
Jdb::is_adapter_memory(Jdb_address)
{
  return 0;
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
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  if (sign && tsc != 0)
    buf->printf("%+lld c", tsc);
  else
    buf->printf("%lld c", tsc);
}

IMPLEMENT_OVERRIDE
void
Jdb::write_tsc(String_buffer *buf, Signed64 tsc, bool sign)
{
  write_tsc_s(buf, tsc, sign);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [mips && mp]:

#include <cstdio>

static
void
Jdb::send_nmi(Cpu_number cpu)
{
  printf("send_nmi to %d not implemented\n",
         cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE inline template< typename T >
void
Jdb::set_monitored_address(T *dest, T val)
{
  *const_cast<T volatile *>(dest) = val;
  Mem::mp_wmb();
}

IMPLEMENT_OVERRIDE inline template< typename T >
T
Jdb::monitor_address(Cpu_number, T volatile const *addr)
{
  return *addr;
}
