
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

  Kobject_common *follow_link(Kobject_common *o) override
  {
    if (Ipc_gate *g = cxx::dyn_cast<Ipc_gate *>(Kobject::from_dbg(o->dbg_info())))
      if (auto ptr = g->target())
        if (auto tgt = Kobject_dbg::pointer_to_obj(ptr); tgt != Kobject_dbg::end())
          return Kobject::from_dbg(*tgt);

    return o;
  }

  bool show_kobject(Kobject_common *, int) override
  { return true; }

  void show_kobject_short(String_buffer *buf, Kobject_common *o, bool) override
  {
    Ipc_gate *g = cxx::dyn_cast<Ipc_gate*>(Kobject::from_dbg(o->dbg_info()));
    if (!g)
      return;

    Kobject_iface *t = nullptr;
    if (auto ptr = g->target())
      {
        if (auto tgt = Kobject_dbg::pointer_to_obj(ptr); tgt != Kobject_dbg::end())
          t = Kobject::from_dbg(*tgt);
      }

    buf->printf(" L=%s%08lx\033[0m D=%lx",
                (g->id() & 3) ? JDB_ANSI_COLOR(lightcyan) : "",
                g->id(), t ? t->dbg_info()->dbg_id() : 0);
  }
};

Jdb_ipc_gate::Jdb_ipc_gate()
  : Jdb_kobject_handler(static_cast<Ipc_gate *>(nullptr))
{
  Jdb_kobject::module()->register_handler(this);
}

static Jdb_ipc_gate jdb_space INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

