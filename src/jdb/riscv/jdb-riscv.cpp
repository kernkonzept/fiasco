IMPLEMENTATION [riscv]:

#include "ipi.h"

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
  printf("Trap in JDB: IP:%08lx Cause=%08lx Status=%08lx\n",
         e->ip(), e->cause, e->status);
}

IMPLEMENT
bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);
  error_buffer.cpu(cpu).clear();

  if (ef->debug_entry_kernel_str())
    error_buffer.cpu(cpu).printf("%s", ef->text());
  else if (ef->debug_entry_user_str())
    error_buffer.cpu(cpu).printf("user \"%.*s\"", ef->textlen(), ef->text());
  else if (ef->debug_ipi())
    error_buffer.cpu(cpu).printf("IPI ENTRY");
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

PUBLIC static FIASCO_INIT FIASCO_NOINLINE
void
Jdb::init()
{
  Thread::nested_trap_handler =
    reinterpret_cast<Trap_state::Handler>(enter_jdb);
  Kconsole::console()->register_console(push_cons());
}

PRIVATE static
unsigned char *
Jdb::access_mem_task(Jdb_address addr, bool write)
{
  if (!Cpu::is_canonical_address(addr.addr()))
    return 0;

  Address phys;

  if (addr.is_kmem())
    {
      phys = Kmem::kdir->virt_to_phys(addr.addr());

      if (phys == Invalid_address)
        return 0;
    }
  else if (!addr.is_phys())
    {
      phys = addr.space()->virt_to_phys_s0(addr.virt());

      if (phys == Invalid_address)
        return 0;
    }
  else
    phys = addr.phys();

  // Pmem is mapped rw, so no need to remap in tmp area.
  Address pmem_addr = Mem_layout::phys_to_pmem(phys);
  if (Mem_layout::in_pmem(pmem_addr))
  {
    auto pte = Kmem::kdir->walk(Virt_addr(pmem_addr));
    if (pte.is_valid() && (!write || pte.attribs().rights & Page::Rights::W()))
      return reinterpret_cast<unsigned char *>(pmem_addr);
  }

  auto pte = Kmem::kdir->walk(Virt_addr(Mem_layout::Jdb_tmp_map_area),
                              Kpdir::Super_level, false,
                              Kmem_alloc::q_allocator(Ram_quota::root));

  if (!pte.is_valid()
      || pte.page_addr() != cxx::mask_lsb(phys, pte.page_order()))
    {
      pte.set_page(Phys_mem_addr(cxx::mask_lsb(phys, pte.page_order())),
                   Page::Attr::space_local(Page::Rights::RW()));

      Mem_unit::tlb_flush();
    }

  return reinterpret_cast<unsigned char *>(
    Mem_layout::Jdb_tmp_map_area + cxx::get_lsb(phys, pte.page_order()));
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
IMPLEMENTATION [riscv && mp]:

#include "warn.h"

static
void
Jdb::send_nmi(Cpu_number cpu)
{
  printf("send_nmi to %d not implemented!\n", cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE template<typename T> inline
void
Jdb::set_monitored_address(T *dest, T val)
{
  *const_cast<T volatile *>(dest) = val;
  Mem::mp_wmb();
}

IMPLEMENT_OVERRIDE template<typename T> inline
T
Jdb::monitor_address(Cpu_number, T volatile const *addr)
{
  return *addr;
}
