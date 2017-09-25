IMPLEMENTATION[arm]:

#include <cstdio>

#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"


//===================
// Std JDB modules
//===================

/**
 * 'IRQ' module.
 * 
 * This module handles the 'e' command that
 * dumps the entry frame.
 */
class Jdb_dump_ef
  : public Jdb_module
{
};

static Jdb_dump_ef jdb_dump_ef INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PUBLIC Jdb_module::Action_code
Jdb_dump_ef::action( int cmd, void *&, char const *&, int & )
{
  if (cmd!=0)
    return NOTHING;

  printf("klr:   %08x  ksp:   %08x  cpsr:  %08x  spsr:  %08x\n", 
         Jdb::entry_frame->pc,
         Jdb::entry_frame->ksp,
         Jdb::entry_frame->cpsr,
         Jdb::entry_frame->spsr);

  for(int i = 0; i<=14; i++)
    {
      printf("r[%2d]: %08x %c", i, Jdb::entry_frame->r[i], (i%4 == 3)?'\n':' ');
    }

  putchar('\n');

  return NOTHING;
}

PUBLIC
int const Jdb_dump_ef::num_cmds() const
{ 
  return 1;
}

PUBLIC
Jdb_module::Cmd const *const Jdb_dump_ef::cmds() const
{
  static Cmd cs[] =
    {   { 0, "e", "entryframe", "\n", 
	   "e\tdump the JDB entry frame", 0 }
    };

  return cs;
}
