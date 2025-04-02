IMPLEMENTATION:

#include <cstdio>
#include <cstring>

#include "thread_object.h"
#include "jdb.h"
#include "jdb_kobject.h"
#include "jdb_module.h"
#include "ipc_gate.h"


class Jdb_sender_list : public Jdb_module, public Jdb_kobject_handler
{
public:
  Jdb_sender_list() FIASCO_INIT;

  bool show_kobject(Kobject_common *, int) override
  { return true; }

private:
  static Kobject *object;
};

static Jdb_sender_list jdb_sender_list INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

Kobject *Jdb_sender_list::object;


PRIVATE
void
Jdb_sender_list::show_sender_list(Prio_list *t,
                                  int overlayprint, int printnone,
                                  const char *tag = nullptr, unsigned long dbgid = 0)
{
  if (overlayprint)
    {
      Jdb::line();
      putchar('\n');
    }

  Prio_list::P_list::Iterator p = t->begin();
  if (p == t->end())
    {
      if (printnone)
        {
          printf("%s (%lx): Nothing in sender list", tag, dbgid);
          if (overlayprint)
            Jdb::clear_to_eol();
          putchar('\n');
        }
      if (overlayprint)
        Jdb::line();
      return;
    }

  bool first = true;
  for (; p != t->end(); ++p)
    {
      if (first)
        {
          printf("%s (%lx): ", tag, dbgid);
          first = false;
        }
      printf("%02x: ", p->prio());
      Prio_list::S_list::Iterator s = Prio_list::S_list::iter(*p);
      do
        {
          Kobject_dbg::Iterator ts = Kobject_dbg::pointer_to_obj(*s);
          printf("%s %lx", *s == *p ? "" : ",",
                 ts != Kobject_dbg::end() ? ts->dbg_id() : ~0UL);
          ++s;
        }
      while (*s != *p);
      if (overlayprint)
        Jdb::clear_to_eol();
      putchar('\n');
    }

  if (overlayprint)
    Jdb::line();
}

PRIVATE
bool
Jdb_sender_list::show_obj(Kobject *o, int printnone)
{
  if (Thread *t = cxx::dyn_cast<Thread *>(o))
    {
      show_sender_list(t->sender_list(), 0, printnone, "Thread", t->dbg_id());
      return true;
    }
  else if (Ipc_gate *g = cxx::dyn_cast<Ipc_gate_obj *>(o))
    {
      show_sender_list(&g->_wait_q, 0, printnone, "Ipc_gate", g->dbg_id());
      return true;
    }

  return false;
}

PUBLIC
Jdb_module::Action_code
Jdb_sender_list::action(int cmd, void *&, char const *&, int &) override
{
  if (cmd)
    return NOTHING;

  if (show_obj(object, 1))
    return NOTHING;

  for (auto const &o : Kobject_dbg::_kobjects)
    show_obj(Kobject::from_dbg(o), 0);

  return NOTHING;
}

PUBLIC
bool
Jdb_sender_list::handle_key(Kobject_common *o, int keycode) override
{
  if (keycode != 'S')
    return false;

  if (Thread *t = cxx::dyn_cast<Thread *>(o))
    show_sender_list(t->sender_list(), 1, 1, "Thread", t->dbg_id());
  else if (Ipc_gate *g = cxx::dyn_cast<Ipc_gate_obj *>(o))
    show_sender_list(&g->_wait_q, 1, 1, "Ipc_gate", g->dbg_id());
  else
    return false;

  Jdb::getchar();
  return true;
}

PUBLIC
char const *
Jdb_sender_list::help_text(Kobject_common *o) const override
{
  if (cxx::dyn_cast<Thread *>(o) || cxx::dyn_cast<Ipc_gate_obj *>(o))
    return "S=sndlist";

  return nullptr;
}

PUBLIC
int Jdb_sender_list::num_cmds() const override
{ return 1; }

PUBLIC
Jdb_module::Cmd const * Jdb_sender_list::cmds() const override
{
  static Cmd cs[] =
    {
	{ 0, "ls", "senderlist", "%q",
          "senderlist\tshow sender-list of thread", &object }
    };

  return cs;
}

IMPLEMENT
Jdb_sender_list::Jdb_sender_list()
  : Jdb_module("INFO")
{
  Jdb_kobject::module()->register_handler(this);
}
