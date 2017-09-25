IMPLEMENTATION [mp]:

#include <cstdio>
#include "simpleio.h"

#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "types.h"

class Jdb_ipi_module : public Jdb_module
{
public:
  Jdb_ipi_module() FIASCO_INIT;
};

static Jdb_ipi_module jdb_ipi_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE static
void
Jdb_ipi_module::print_info(Cpu_number cpu)
{
  Ipi &ipi = Ipi::_ipi.cpu(cpu);
  printf("CPU%02u sent/rcvd: %lu/%lu\n",
         cxx::int_value<Cpu_number>(cpu), ipi._stat_sent, ipi._stat_received);
}

PUBLIC
Jdb_module::Action_code
Jdb_ipi_module::action(int cmd, void *&, char const *&, int &)
{
  if (cmd)
    return NOTHING;

  Jdb::foreach_cpu(&print_info);

  return NOTHING;
}

PUBLIC
int
Jdb_ipi_module::num_cmds() const
{ return 1; }

PUBLIC
Jdb_module::Cmd const *
Jdb_ipi_module::cmds() const
{
  static Cmd cs[] =
    { { 0, "", "ipi", "", "ipi\tIPI information", 0 } };

  return cs;
}

IMPLEMENT
Jdb_ipi_module::Jdb_ipi_module()
  : Jdb_module("INFO")
{}
