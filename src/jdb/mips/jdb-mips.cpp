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
  const char *str = "<break>";
  bool from_user = ef->status & Trap_state::S_ksu;

  if (!from_user)
    {
      // kernel kdb_ke provides a string pointer in a0
      auto x = (char const *)ef->r[Entry_frame::R_a0];
      if ((uintptr_t)x >= Mem_layout::KSEG0
          && (uintptr_t)x <= Mem_layout::KSEG0e
          && memchr(x, 0, 100))
        str = x;
    }

  if (ef->debug_ipi())
    error_buffer.cpu(cpu).printf("IPI ENTRY");
  else if (ef->debug_trap())
    error_buffer.cpu(cpu).printf("%s", str);
  else
    error_buffer.cpu(cpu).printf("ENTRY");

  return true;
}

IMPLEMENT inline
bool
Jdb::handle_user_request(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);
  const char *str = (char const *)ef->r[Entry_frame::R_a0];
  Space * task = get_task(cpu);

  if (ef->debug_ipi())
    return cpu != Cpu_number::boot_cpu();

  if (ef->debug_sequence())
    return execute_command_ni(task, str);

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
void *
Jdb::access_mem_task(Address virt, Space * task)
{
  // align
  virt &= ~0x03;

  Address phys;
  if (task)
    {
      phys = task->virt_to_phys_s0((void *)virt);

      if (phys == (Address)-1)
        return 0;
    }
  else
    {
      if (virt >= Mem_layout::KSEG0 && virt <= Mem_layout::KSEG0e)
        return (void*)virt;

      phys = virt;
    }

  // physical memory accessible via unmapped KSEG0
  if (phys <= Mem_layout::KSEG0e - Mem_layout::KSEG0)
    return (void *)(phys + Mem_layout::KSEG0);

  // FIXME: temp mapping for the physical memory needed
  return 0;
}

PUBLIC static
Space *
Jdb::translate_task(Address addr, Space * task)
{
  return (Kmem::is_kmem_page_fault(addr, 0)) ? 0 : task;
}

PUBLIC static
int
Jdb::peek_task(Address virt, Space * task, void *value, int width)
{

  void const *mem = access_mem_task(virt, task);
  if (!mem)
    return -1;

  switch (width)
    {
    case 1:
        {
          Mword dealign = (virt & 0x3) * 8;
          *(Mword*)value = (*(Mword*)mem & (0xff << dealign)) >> dealign;
          break;
        }
    case 2:
        {
          Mword dealign = ((virt & 0x2) >> 1) * 16;
          *(Mword*)value = (*(Mword*)mem & (0xffff << dealign)) >> dealign;
          break;
        }
    case 4:
    case 8:
      memcpy(value, mem, width);
      break;
    }

  return 0;
}

PUBLIC static
int
Jdb::is_adapter_memory(Address, Space *)
{
  return 0;
}

PUBLIC static
int
Jdb::poke_task(Address virt, Space * task, void const *val, int width)
{
  void *mem = access_mem_task(virt, task);
  if (!mem)
    return -1;

  memcpy(mem, val, width);
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

PUBLIC static
void
Jdb::write_tsc_s(String_buffer *buf, Signed64 tsc, bool sign)
{
  if (sign)
    buf->printf("%c", (tsc < 0) ? '-' : (tsc == 0) ? ' ' : '+');
  buf->printf("%lld c", tsc);
}

PUBLIC static
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
