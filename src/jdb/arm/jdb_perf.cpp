INTERFACE [arm]:

#include "jdb_module.h"

class Jdb_perf : public Jdb_module
{
public:
  Jdb_perf() FIASCO_INIT;
};

// ----------------------------------------------------------------
IMPLEMENTATION [arm]:

#include "perf_cnt.h"
#include "static_init.h"

#include <cstdio>

PUBLIC
Jdb_module::Action_code
Jdb_perf::action(int cmd, void *&, char const *&, int &)
{
  if (cmd)
    return NOTHING;

  printf("\n");
  Mword val = Perf_cnt::read_cycle_cnt();;
  printf("Cycle counter: %08lx / %10lu\n", val, val);
  for (unsigned i = 0; i < 8; ++i)
    {
      val = Perf_cnt::read_counter(i);
      printf("Event counter %d, type=%03d: %08lx / %10lu\n",
             i, Perf_cnt::mon_event_type(i), val, val);
    }

  return NOTHING;
}

PUBLIC
int
Jdb_perf::num_cmds() const
{ return 1; }


PUBLIC
Jdb_module::Cmd const *
Jdb_perf::cmds() const
{
  static Cmd cs[] =
    {
      { 0, "M", "monperf", "", "M\tPerformance monitor events", 0 },
    };
  return cs;
}

IMPLEMENT
Jdb_perf::Jdb_perf()
  : Jdb_module("INFO")
{
}

static Jdb_perf jdb_perf INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
