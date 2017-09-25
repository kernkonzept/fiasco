IMPLEMENTATION:

#include <cstdio>
#include <cstring>
#include "config.h"
#include "jdb_tbuf.h"
#include "jdb_module.h"
#include "kern_cnt.h"
#include "simpleio.h"
#include "static_init.h"

class Jdb_counters : public Jdb_module
{
public:
  Jdb_counters() FIASCO_INIT;
private:
  static char counters_cmd;
};

char Jdb_counters::counters_cmd;

PRIVATE
void
Jdb_counters::show()
{
  putchar('\n');
  for (unsigned i=0; i<Kern_cnt_max; i++)
    printf("  %-25s%10lu\n", Kern_cnt::get_str(i), *Kern_cnt::get_ctr(i));
  putchar('\n');
}

PRIVATE
void
Jdb_counters::reset()
{
  memset(Jdb_tbuf::status()->kerncnts, 0, 
         sizeof(Jdb_tbuf::status()->kerncnts));
}

PUBLIC
Jdb_module::Action_code
Jdb_counters::action(int cmd, void *&, char const *&, int &)
{
  if (!Config::Jdb_accounting)
    {
      puts(" accounting disabled");
      return ERROR;
    }

  if (cmd == 0)
    {
      switch (counters_cmd)
	{
	case 'l':
	  show();
	  break;
	case 'r':
	  reset();
	  putchar('\n');
	  break;
	}
    }
  return NOTHING;
}

PUBLIC
Jdb_counters::Cmd const *
Jdb_counters::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "C", "cnt", "%c",
	  "C{l|r}\tshow/reset kernel event counters", &counters_cmd },
    };
  return cs;
}

PUBLIC
int
Jdb_counters::num_cmds() const
{
  return 1;
}

IMPLEMENT
Jdb_counters::Jdb_counters()
  : Jdb_module("MONITORING")
{}

static Jdb_counters jdb_counters INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
