IMPLEMENTATION:

#include <cstdio>
#include "entry_frame.h"
#include "gdt.h"
#include "jdb_module.h"
#include "jdb_kobject.h"
#include "static_init.h"
#include "thread_object.h"

class Jdb_halt_thread : public Jdb_module
{
public:
  Jdb_halt_thread() FIASCO_INIT;
private:
  static Kobject *threadid;
};

Kobject *Jdb_halt_thread::threadid;

PUBLIC
Jdb_module::Action_code
Jdb_halt_thread::action(int cmd, void *&, char const *&, int &)
{
  if (cmd != 0)
    return NOTHING;

  Thread *t = cxx::dyn_cast<Thread*>(threadid);

  if (!t)
    return NOTHING;

  t->regs()->cs(Gdt::gdt_code_kernel | Gdt::Selector_kernel);
  t->regs()->ip(reinterpret_cast<Address>(&Thread::halt_current));
  t->regs()->flags(0);  // disable interrupts
  putchar('\n');

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_halt_thread::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "H", "halt", "%q", "H<threadid>\thalt a specific thread",
	  &threadid },
    };

  return cs;
}

PUBLIC
int
Jdb_halt_thread::num_cmds() const
{
  return 1;
}

IMPLEMENT
Jdb_halt_thread::Jdb_halt_thread()
  : Jdb_module("MISC")
{}

static Jdb_halt_thread jdb_halt_thread INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
