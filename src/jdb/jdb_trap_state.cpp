IMPLEMENTATION:

#include <cstdio>
#include "simpleio.h"

#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"


/**
 * Private 'exit' module.
 * 
 * This module handles the 'exit' or '^' command that
 * makes a call to exit() and virtually reboots the system.
 */
class Jdb_trap_state_module : public Jdb_module
{
public:
  Jdb_trap_state_module() FIASCO_INIT;
};

static Jdb_trap_state_module jdb_trap_state_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);


PRIVATE static
void
Jdb_trap_state_module::print_trap_state(Cpu_number cpu)
{
  Jdb_entry_frame *ef = Jdb::entry_frame.cpu(cpu);
  if (!Jdb::cpu_in_jdb(cpu) || !ef)
    printf("CPU %u has not entered JDB\n", cxx::int_value<Cpu_number>(cpu));
  else
    {
      printf("Registers of CPU %u (before entering JDB)\n",
             cxx::int_value<Cpu_number>(cpu));
      ef->dump();
    }
}

PUBLIC
Jdb_module::Action_code
Jdb_trap_state_module::action (int cmd, void *&argbuf, char const *&fmt, int &next)
{
  char const *c = (char const *)argbuf;
  static Cpu_number cpu;

  if (cmd != 0)
    return NOTHING;

  if (argbuf != &cpu)
    {
      if (*c == 'a')
	Jdb::foreach_cpu(&print_trap_state);
      else if (*c >= '0' && *c <= '9')
	{
	  next = *c; argbuf = &cpu; fmt = "%i";
	  return EXTRA_INPUT_WITH_NEXTCHAR;
	}
    }
  else
    print_trap_state(cpu);

  return NOTHING;
}

PUBLIC
int
Jdb_trap_state_module::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_trap_state_module::cmds() const
{
  static char c;
  static Cmd cs[] =
    { { 0, "", "cpustate", "%C", "cpustate all|<cpunum>\tdump state of CPU", &c } };

  return cs;
}

IMPLEMENT
Jdb_trap_state_module::Jdb_trap_state_module()
  : Jdb_module("INFO")
{}
