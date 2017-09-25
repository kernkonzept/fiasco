IMPLEMENTATION:

#include <cstdio>
#include "simpleio.h"

#include "apic.h"
#include "io_apic.h"
#include "jdb.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "static_init.h"
#include "types.h"


/**
 * Private 'exit' module.
 * 
 * This module handles the 'exit' or '^' command that
 * makes a call to exit() and virtually reboots the system.
 */
class Jdb_io_apic_module : public Jdb_module
{
public:
  Jdb_io_apic_module() FIASCO_INIT;
};

static Jdb_io_apic_module jdb_io_apic_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE static
void
Jdb_io_apic_module::print_lapic(Cpu_number cpu)
{
  printf("\nLocal APIC [%u, %08x]: tpr=%2x ppr=%2x\n",
         cxx::int_value<Cpu_number>(cpu),
         Apic::get_id(), Apic::tpr(), Apic::reg_read(0xa0));
  printf("  Running: tpr=%02x\n", Jdb::apic_tpr.cpu(cpu));

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
Jdb_module::Action_code
Jdb_io_apic_module::action (int cmd, void *&, char const *&, int &)
{
  if (cmd!=0)
    return NOTHING;

  if (!Io_apic::active())
    {
      printf("\nIO APIC not present!\n");
      return NOTHING;
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
  Jdb::on_each_cpu(print_lapic);

  return NOTHING;
}

PUBLIC
int
Jdb_io_apic_module::num_cmds() const
{ 
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_io_apic_module::cmds() const
{
  static Cmd cs[] =
    { { 0, "A", "apic", "", "apic\tdump state of IOAPIC", (void*)0 } };

  return cs;
}

IMPLEMENT
Jdb_io_apic_module::Jdb_io_apic_module()
  : Jdb_module("INFO")
{}
