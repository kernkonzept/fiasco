IMPLEMENTATION [mp]:

#include <cstdio>
#include "simpleio.h"

#include "cpu_call.h"
#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"

class Jdb_cpu_call_module : public Jdb_module
{
public:
  Jdb_cpu_call_module() FIASCO_INIT;
};

static Jdb_cpu_call_module jdb_cpu_call_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE static
void
Jdb_cpu_call_module::print_info(Cpu_number cpu)
{
  Cpu_call_queue &q = Cpu_call::_glbl_q.cpu(cpu);
  Queue_item *qi = q.first();
  if (qi)
    {
      Cpu_call *r = static_cast<Cpu_call*>(qi);
      printf("CPU%02u wait: %ld, queued: %d\n",
             cxx::int_value<Cpu_number>(cpu), r->_wait, r->queued());
    }
}

PUBLIC
Jdb_module::Action_code
Jdb_cpu_call_module::action(int cmd, void *&, char const *&, int &) override
{
  if (cmd)
    return NOTHING;

  Jdb::foreach_cpu(&print_info);

  return NOTHING;
}

PUBLIC
int
Jdb_cpu_call_module::num_cmds() const override
{ return 1; }

PUBLIC
Jdb_module::Cmd const *
Jdb_cpu_call_module::cmds() const override
{
  static Cmd cs[] =
    { { 0, "", "cpucalls", "", "cpucalls\tshow CPU calls", 0 } };

  return cs;
}

IMPLEMENT
Jdb_cpu_call_module::Jdb_cpu_call_module()
  : Jdb_module("INFO")
{}
