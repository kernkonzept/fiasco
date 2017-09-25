IMPLEMENTATION:

#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_kobject.h"
#include "simpleio.h"
#include "static_init.h"
#include "ipc_gate.h"

class Jdb_ipc_gate : public Jdb_kobject_handler
{
public:
  Jdb_ipc_gate() FIASCO_INIT;
};

IMPLEMENT
Jdb_ipc_gate::Jdb_ipc_gate()
  : Jdb_kobject_handler((Ipc_gate_obj*)0)
{
  Jdb_kobject::module()->register_handler(this);
}

PUBLIC
Kobject_common *
Jdb_ipc_gate::follow_link(Kobject_common *o)
{
  Ipc_gate_obj *g = cxx::dyn_cast<Ipc_gate_obj *>(Kobject::from_dbg(o->dbg_info()));
  return g->thread() ? Kobject::from_dbg(g->thread()->dbg_info()) : o;
}

PUBLIC
bool
Jdb_ipc_gate::show_kobject(Kobject_common *, int)
{ return true; }

PUBLIC
void
Jdb_ipc_gate::show_kobject_short(String_buffer *buf, Kobject_common *o)
{
  Ipc_gate_obj *g = cxx::dyn_cast<Ipc_gate_obj*>(Kobject::from_dbg(o->dbg_info()));
  if (!g)
    return;

  buf->printf(" L=%s%08lx\033[0m D=%lx",
              (g->id() & 3) ? JDB_ANSI_COLOR(lightcyan) : "",
              g->id(), g->thread() ? g->thread()->dbg_info()->dbg_id() : 0);
}

static Jdb_ipc_gate jdb_space INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

