INTERFACE [arm]:

EXTENSION class Jdb
{
public:
  static int (*bp_test_log_only)(Cpu_number);
  static int (*bp_test_break)(Cpu_number, String_buffer *);
};

IMPLEMENTATION [arm]:

#include "globals.h"
#include "kernel_task.h"
#include "kmem_alloc.h"
#include "kmem.h"
#include "space.h"
#include "mem_layout.h"
#include "mem_unit.h"
#include "static_init.h"
#include "timer_tick.h"
#include "watchdog.h"
#include "cxx/cxx_int"

STATIC_INITIALIZE_P(Jdb, JDB_INIT_PRIO);

DEFINE_PER_CPU static Per_cpu<Proc::Status> jdb_irq_state;

int (*Jdb::bp_test_log_only)(Cpu_number);
int (*Jdb::bp_test_break)(Cpu_number, String_buffer *buf);

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && !pic_gic]:

PRIVATE static inline
void
Jdb::wfi_enter()
{}

PRIVATE static inline
void
Jdb::wfi_leave()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && serial]:

PRIVATE static inline
void
Jdb::kernel_uart_irq_ack()
{ Kernel_uart::uart()->irq_ack(); }

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && !serial]:

PRIVATE static inline
void
Jdb::kernel_uart_irq_ack()
{}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

#include "gic.h"

struct Jdb_wfi_gic
{
  unsigned orig_tt_prio;
  unsigned orig_pmr;
};
static Jdb_wfi_gic wfi_gic;

PRIVATE static
void
Jdb::wfi_enter()
{
  Jdb_core::wait_for_input = _wait_for_input;

  Timer_tick *tt = Timer_tick::boot_cpu_timer_tick();
  Gic *g = static_cast<Gic*>(tt->chip());

  wfi_gic.orig_tt_prio = g->irq_prio_bootcpu(tt->pin());
  wfi_gic.orig_pmr     = g->get_pmr();
  g->set_pmr(0x90);
  g->irq_prio_bootcpu(tt->pin(), 0x00);

  Timer_tick::enable(Cpu_number::boot_cpu());
}

PRIVATE static
void
Jdb::wfi_leave()
{
  Timer_tick *tt = Timer_tick::boot_cpu_timer_tick();
  Gic *g = static_cast<Gic*>(tt->chip());
  g->irq_prio_bootcpu(tt->pin(), wfi_gic.orig_tt_prio);
  g->set_pmr(wfi_gic.orig_pmr);
}

PRIVATE static
void
Jdb::_wait_for_input()
{
  Proc::halt();

  Timer_tick *tt = Timer_tick::boot_cpu_timer_tick();
  unsigned i = static_cast<Gic*>(tt->chip())->get_pending();
  if (i == tt->pin())
    {
      kernel_uart_irq_ack();
      tt->ack();
    }
  else
    printf("JDB: Unexpected interrupt %d\n", i);
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "timer.h"

// disable interrupts before entering the kernel debugger
IMPLEMENT
void
Jdb::save_disable_irqs(Cpu_number cpu)
{
  jdb_irq_state.cpu(cpu) = Proc::cli_save();
  if (cpu == Cpu_number::boot_cpu())
    Watchdog::disable();

  Timer_tick::disable(cpu);

  if (cpu == Cpu_number::boot_cpu())
    wfi_enter();
}

// restore interrupts after leaving the kernel debugger
IMPLEMENT
void
Jdb::restore_irqs(Cpu_number cpu)
{
  if (cpu == Cpu_number::boot_cpu())
    wfi_leave();

  Timer_tick::enable(cpu);

  if (cpu == Cpu_number::boot_cpu())
    Watchdog::enable();
  Proc::sti_restore(jdb_irq_state.cpu(cpu));
}

IMPLEMENT inline NEEDS["timer.h"]
void
Jdb::enter_trap_handler(Cpu_number)
{ Timer::switch_freq_jdb(); }

IMPLEMENT inline NEEDS["timer.h"]
void
Jdb::leave_trap_handler(Cpu_number)
{ Timer::switch_freq_system(); }

IMPLEMENT inline
bool
Jdb::handle_conditional_breakpoint(Cpu_number cpu, Jdb_entry_frame *e)
{
  return Thread::is_debug_exception(e->error_code)
         && bp_test_log_only && bp_test_log_only(cpu);
}

IMPLEMENT
void
Jdb::handle_nested_trap(Jdb_entry_frame *e)
{
  printf("Trap in JDB: IP:%08lx PSR=%08lx ERR=%08lx\n",
         e->ip(), e->psr, e->error_code);
}

IMPLEMENT
bool
Jdb::handle_debug_traps(Cpu_number cpu)
{
  Jdb_entry_frame *ef = entry_frame.cpu(cpu);
  error_buffer.cpu(cpu).clear();

  if (Thread::is_debug_exception(ef->error_code)
      && bp_test_break)
    return bp_test_break(cpu, &error_buffer.cpu(cpu));

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
  if (!Cpu::is_canonical_address(addr.addr()))
    return 0;

  Address phys;

  if (addr.is_kmem())
    {
      auto p = Kmem::kdir->walk(Virt_addr(addr.addr()));
      if (!p.is_valid())
        return 0;

      phys = p.page_addr() | cxx::get_lsb(addr.addr(), p.page_order());
    }
  else if (!addr.is_phys())
    {
      phys = Address(addr.space()->virt_to_phys_s0(addr.virt()));

      if (phys == (Address)-1)
        return 0;
    }
  else
    phys = addr.phys();

  unsigned long kaddr = Mem_layout::phys_to_pmem(phys);
  if (kaddr != (Address)-1)
    {
      auto pte = Kmem::kdir->walk(Virt_addr(kaddr));
      if (pte.is_valid()
          && (!write || pte.attribs().rights & Page::Rights::W()))
        return (unsigned char *)kaddr;
    }

  Mem_unit::flush_vdcache();
  auto pte = Kmem::kdir
    ->walk(Virt_addr(Mem_layout::Jdb_tmp_map_area), K_pte_ptr::Super_level);

  if (!pte.is_valid()
      || pte.page_addr() != cxx::mask_lsb(phys, pte.page_order()))
    {
          Page::Type mem_type = Page::Type::Uncached();
          for (auto const &md: Kip::k()->mem_descs_a())
            if (!md.is_virtual() && md.contains(phys)
                && (md.type() == Mem_desc::Conventional))
              {
                mem_type = Page::Type::Normal();
                break;
              }

          pte.set_page(
            pte.make_page(Phys_mem_addr(cxx::mask_lsb(phys, pte.page_order())),
                          Page::Attr(Page::Rights::RW(), mem_type)));
          pte.write_back_if(true, Mem_unit::Asid_kernel);
    }

  Mem_unit::kernel_tlb_flush();

  return (unsigned char *)(Mem_layout::Jdb_tmp_map_area
                           + (phys & (Config::SUPERPAGE_SIZE - 1)));
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
  Mem_unit::clean_vdcache();
}

PRIVATE static
void
Jdb::at_jdb_leave()
{
  Mem_unit::flush_vcache();
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
IMPLEMENTATION [arm && mp]:

#include <cstdio>

static
void
Jdb::send_nmi(Cpu_number cpu)
{
  printf("NMI to %d, what's that?\n",
         cxx::int_value<Cpu_number>(cpu));
}

IMPLEMENT_OVERRIDE inline template< typename T >
void
Jdb::set_monitored_address(T *dest, T val)
{
  *const_cast<T volatile *>(dest) = val;
  Mem::dsb();
  asm volatile("sev");
}

IMPLEMENT_OVERRIDE inline template< typename T >
T
Jdb::monitor_address(Cpu_number, T volatile const *addr)
{
  asm volatile("wfe");
  return *addr;
}

IMPLEMENT_OVERRIDE
void
Jdb::other_cpu_halt_in_jdb()
{
  Proc::halt();
}

IMPLEMENT_OVERRIDE
void
Jdb::wakeup_other_cpus_from_jdb(Cpu_number c)
{
  Ipi::send(Ipi::Debug, Cpu_number::first(), c);
}
