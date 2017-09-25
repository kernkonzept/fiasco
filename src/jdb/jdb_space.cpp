IMPLEMENTATION:

#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_core.h"
#include "jdb_module.h"
#include "jdb_screen.h"
#include "jdb_kobject.h"
#include "kernel_console.h"
#include "kernel_task.h"
#include "keycodes.h"
#include "ram_quota.h"
#include "simpleio.h"
#include "task.h"
#include "thread_object.h"
#include "static_init.h"

class Jdb_space : public Jdb_module, public Jdb_kobject_handler
{
public:
  Jdb_space() FIASCO_INIT;
private:
  static Task *task;
};

Task *Jdb_space::task;

IMPLEMENT
Jdb_space::Jdb_space()
  : Jdb_module("INFO"), Jdb_kobject_handler((Task*)0)
{
  Jdb_kobject::module()->register_handler(this);
}

PUBLIC
bool
Jdb_space::show_kobject(Kobject_common *o, int lvl)
{
  Task *t = cxx::dyn_cast<Task*>(o);
  show(t);
  if (lvl)
    {
      Jdb::getchar();
      return true;
    }

  return false;
}

PUBLIC
void
Jdb_space::show_kobject_short(String_buffer *buf, Kobject_common *o)
{
  Task *t = cxx::dyn_cast<Task*>(o);
  if (t == Kernel_task::kernel_task())
    buf->printf(" {KERNEL}");

  buf->printf(" R=%ld", t->ref_cnt());
}

PRIVATE static
void
Jdb_space::print_space(Space *s)
{
  printf("%p", s);
}

PRIVATE
void
Jdb_space::show(Task *t)
{
  printf("Space %p (Kobject*)%p\n", t, static_cast<Kobject*>(t));

  for (Space::Ku_mem_list::Const_iterator m = t->_ku_mem.begin(); m != t->_ku_mem.end();
       ++m)
    printf("  utcb area: user_va=%p kernel_va=%p size=%x\n",
           m->u_addr.get(), m->k_addr, m->size);

  unsigned long m = t->ram_quota()->current();
  unsigned long l = t->ram_quota()->limit();
  printf("  mem usage:  %lu (%luKB) of %lu (%luKB) @%p\n", 
         m, m/1024, l, l/1024, t->ram_quota());
}

static bool space_filter(Kobject_common const *o)
{ return cxx::dyn_cast<Task const *>(o); }

PUBLIC
Jdb_module::Action_code
Jdb_space::action(int cmd, void *&, char const *&, int &)
{
  if (cmd == 0)
    {
      Jdb_kobject_list list(space_filter);
      list.do_list();
    }
  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_space::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "s", "spacelist", "", "s\tshow task list", 0 },
    };
  return cs;
}
  
PUBLIC
int
Jdb_space::num_cmds() const
{ return 1; }

static
bool
filter_task_thread(Kobject_common const *o)
{
  return cxx::dyn_cast<Task const *>(o) || cxx::dyn_cast<Thread const *>(o);
}
static Jdb_space jdb_space INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[Tasks + Threads]", filter_task_thread);

