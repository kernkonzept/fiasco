IMPLEMENTATION [tickless_idle]:

#include <climits>
#include <cstring>
#include <cstdio>

#include "jdb.h"
#include "jdb_module.h"
#include "kernel_thread.h"
#include "static_init.h"


class Jdb_idle_stats : public Jdb_module
{
public:
  Jdb_idle_stats() FIASCO_INIT;
};

IMPLEMENT
Jdb_idle_stats::Jdb_idle_stats() : Jdb_module("INFO") {}

PUBLIC
Jdb_module::Action_code
Jdb_idle_stats::action(int, void *&, char const *&, int &)
{
  printf("\nIDLE STATISTICS --------------------------\n");
  for (Cpu_number i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      if (!Cpu::online(i))
        continue;

      printf("CPU[%2u]: %lu times idle, %lu times deep sleep\n",
             cxx::int_value<Cpu_number>(i),
             Kernel_thread::_idle_counter.cpu(i),
             Kernel_thread::_deep_idle_counter.cpu(i));
    }
  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_idle_stats::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, 0, "idle", "", "idle\tshow IDLE statistics", 0},
    };
  return cs;
}

PUBLIC
int
Jdb_idle_stats::num_cmds() const
{ return 1; }

static Jdb_idle_stats jdb_idle_stats INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
