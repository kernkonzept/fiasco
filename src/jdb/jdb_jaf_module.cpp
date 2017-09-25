IMPLEMENTATION:

#include <cstdio>

#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"
#include "cpu.h"
#include "globals.h"
#include "context.h"

/**
 * Jdb-bsod module
 *
 * This module makes fun.
 */
class Bsod_m 
  : public Jdb_module
{
public:
  Bsod_m() FIASCO_INIT;
};

static Bsod_m bsod_m INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

IMPLEMENT
Bsod_m::Bsod_m()
  : Jdb_module("GENERAL")
{}

PUBLIC
Jdb_module::Action_code Bsod_m::action( int, void *&, char const *& )
{
  const char *bsod_unstable =
    "\033[H\033[44;37;1m\033[2J\n\n\n\n\n\n"
    "                                \033[47;34;1m WARNING! \033[44;37;1m\n\n"
    "    The system is either busy or has become unstable. You can wait and\n"
    "    see if it becomes available again, or you can restart your computer.\n\n"
    "    * Press any key to return to Fiasco and wait.\n"
    "    * Press '^' to restart your computer. You will\n"
    "      lose unsaved information in any programs that are running.\n\n"
    "                        Press any key to continue \n"
    "\n\n\n\n\n\n\n\n                                                                              "
    "\033[16;51H";

  const char *bsod_dll =
    "\033[H\033[44;37;1m\033[2J\n\n\n\n\n\n"
    "                                \033[47;34;1m Fiasco \033[44;37;1m\n\n"
    "    A fatal exception OE has occurred at %x in JDB.dll + \n"
    "    %x. The current application will be terminated.\n\n"
    "    * Press any key to terminate the current application.\n"
    "    * Press '^' to restart your computer. You will\n"
    "      lose any unsaved information in all applications.\n\n"
    "                         Press any key to continue \n"
    "\n\n\n\n\n\n\n\n                                                                              "
    "\033[16;52H";

  Unsigned64 tsc = Cpu::rdtsc ();

  if (tsc & 0x10)
    printf(bsod_unstable);
  else
    printf(bsod_dll, current()->regs()->pc());

  getchar();

  return NOTHING;
}

PUBLIC
int const Bsod_m::num_cmds() const
{ 
  return 1;
}

PUBLIC
Jdb_module::Cmd const *const Bsod_m::cmds() const
{
  static Cmd cs[] =
    { { 0, "t", "t", "\n", NULL, 0 }, };

  return cs;
}
