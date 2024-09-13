IMPLEMENTATION:

#include <cstdio>
#include "simpleio.h"

#include "apic.h"
#include "io_apic.h"
#include "jdb_kern_info.h"
#include "static_init.h"

class Jdb_kern_info_io_apic : public Jdb_kern_info_module
{
};

static Jdb_kern_info_io_apic k_A INIT_PRIORITY(JDB_MODULE_INIT_PRIO+1);

PUBLIC
Jdb_kern_info_io_apic::Jdb_kern_info_io_apic()
  : Jdb_kern_info_module('A', "I/O APIC state")
{
  Jdb_kern_info::register_subcmd(this);
}

PRIVATE static
void
Jdb_kern_info_io_apic::print_lapic(Cpu_number cpu)
{
  printf("\nLocal APIC [%u, %08x]: tpr=%2x ppr=%2x\n"
         "  Running: tpr=%02x\n"
         "  Timer: icr=%08x ccr=%08x LVT=%08x\n",
         cxx::int_value<Cpu_number>(cpu), cxx::int_value<Apic_id>(Apic::get_id()),
         Apic::tpr(), Apic::reg_read(0xa0), Jdb::apic_tpr.cpu(cpu),
         Apic::reg_read(0x380), Apic::reg_read(0x390), Apic::reg_read(0x320));

  unsigned const regs[] = { 0x200, 0x100, 0x180 };
  char const *const regn[] = { "IRR", "ISR", "TMR" };
  for (unsigned r = 0; r < 3; ++r)
    {
      printf("  %s:", regn[r]);
      for (int i = 3; i >= 0; --i)
        {
          unsigned long v = Apic::reg_read(regs[r] + i * 0x10);
          printf(" %08lx", v);
	}
      puts("");
    }
}

PUBLIC
void
Jdb_kern_info_io_apic::show() override
{
  if (!Io_apic::active())
    {
      printf("\nIO APIC not present!\n");
      return;
    }

  printf("\nState of IO APIC\n");
  for (Io_apic *a = Io_apic::_first; a; a = a->_next)
    a->dump();

  // print global LAPIC state
  unsigned khz;
  char apic_state[80];
  int apic_disabled;
  strcpy (apic_state, "N/A");
  if ((apic_disabled = Apic::test_present_but_disabled()))
    strcpy (apic_state, "disabled by BIOS");
  if ((khz = Apic::get_frequency_khz()))
    {
      unsigned mhz = khz / 1000;
      khz -= mhz * 1000;
      snprintf(apic_state, sizeof(apic_state), "yes (%u.%03u MHz)"
	      "\n  local APIC spurious interrupts/bug/error: %u/%u/%u",
	      mhz, khz,
	      apic_spurious_interrupt_cnt,
	      apic_spurious_interrupt_bug_cnt,
	      apic_error_cnt);
    }
  printf("\nLocal APIC (general): %s"
	 "\nWatchdog: %s"
	 "\n",
	 apic_state,
	 Config::watchdog
	    ? "active"
	    : Apic::is_present()
	       ? "disabled"
	       : apic_disabled
	          ? "not supported (Local APIC disabled)"
		  : "not supported (no Local APIC)"
      );
  Jdb::on_each_cpu(&print_lapic);
}
