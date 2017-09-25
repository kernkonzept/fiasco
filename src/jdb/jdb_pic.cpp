IMPLEMENTATION:

#include <cstdio>

#include "pic.h"
#include "jdb_module.h"
#include "jdb_handler_queue.h"
#include "static_init.h"
#include "types.h"


//===================
// Std JDB modules
//===================

/**
 * 'IRQ' module.
 *
 * This module handles the 'R' command that
 * provides IRQ attachment and listing functions.
 */
class Jdb_pic
  : public Jdb_module
{
};

static Jdb_pic jdb_pic INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

static Pic::Status pic_state;

PRIVATE static
void Jdb_pic::at_enter()
{
  pic_state = Pic::disable_all_save();
}

PRIVATE static
void Jdb_pic::at_leave()
{
  Pic::restore_all(pic_state);
}

PUBLIC
Jdb_pic::Jdb_pic()
{
  static Jdb_handler enter(at_enter);
  static Jdb_handler leave(at_leave);
  register_handlers(enter,leave);
}


PUBLIC Jdb_module::Action_code
Jdb_pic::action( int cmd, void *&/*args*/, char const *&/*fmt*/, int & )
{
  if (cmd!=0)
    return NOTHING;

  printf("PIC state: %08x\n", pic_state);

  return NOTHING;
}

PUBLIC
int Jdb_pic::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *Jdb_pic::cmds() const
{
  static Cmd cs[] =
    {{ 0, "i", "pic", "", "i\tshow pic state", 0 }};

  return cs;
}

IMPLEMENTATION [jdb]:

#include "jdb.h"

PRIVATE
void
Jdb_pic::register_handlers( Jdb_handler &enter, Jdb_handler &leave )
{
  Jdb::jdb_enter.add(&enter);
  Jdb::jdb_leave.add(&leave);
}

