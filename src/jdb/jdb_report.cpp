IMPLEMENTATION:

#include <cstdio>

#include "kobject.h"
#include "string_buffer.h"

#include "jdb.h"
#include "jdb_module.h"
#include "jdb_kobject.h"
#include "jdb_screen.h"


class Jdb_report : public Jdb_module
{
  static char subcmd;

public:
  Jdb_report() : Jdb_module("INFO") {}
};

char Jdb_report::subcmd;

static Jdb_report jdb_report INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PRIVATE
void
Jdb_report::show_objects()
{
  String_buf<256> buf;

  printf("System objects:\n");
  for (auto const &&l: Kobject_dbg::_kobjects)
    {
      buf.reset();
      Jdb_kobject::obj_description(&buf, nullptr, false, l);
      printf("%.*s\n", buf.length(), buf.c_str());
    }
}

void
Jdb_report::delim(const char *text = 0)
{
  puts(Jdb_screen::Line);
  if (text)
    puts(text);
}

PUBLIC
Jdb_module::Action_code
Jdb_report::action(int cmd, void *&, char const *&, int &) override
{
  if (cmd != 0)
    return NOTHING;

  unsigned orig_height = Jdb_screen::height();
  unsigned orig_width  = Jdb_screen::width();
  Jdb_screen::set_height(9999);
  Jdb_screen::set_width(999);

  Console *gzip = 0;
  if (subcmd == 'c' || subcmd == 'C')
    gzip = Kconsole::console()->find_console(Console::GZIP);

  if (gzip)
    Kconsole::console()->start_exclusive(Console::GZIP);

  delim("Objects:");
  show_objects();

  delim("Threads [present]:");
  Jdb::execute_command_long("threadlist p");
  delim("Threads [ready]:");
  Jdb::execute_command_long("threadlist r");
  delim("Timeouts:");
  Jdb::execute_command_long("timeoutsdump");


  delim("Kmem:");
  Jdb::execute_command_long("k m");
  delim("Kernel info:");
  Jdb::execute_command_long("k c");
  Jdb::execute_command_long("k f");

  delim("IPI");
  Jdb::execute_command_long("ipi");

  delim("CPU calls");
  Jdb::execute_command_long("cpucalls");

  delim("RCU");
  Jdb::execute_command_long("rcupdate");

  delim("Trace buffer:");
  Jdb::execute_command_long("tbufdumptext");

  delim("Console output:");
  Jdb::execute_command_long("consolebuffer 1000");

  delim("TCBs:");
  Jdb_screen::set_width(80);
  for (auto const &&l: Kobject_dbg::_kobjects)
    {
      Thread *t = cxx::dyn_cast<Thread *>(Kobject::from_dbg(l));
      if (t)
        {
          String_buf<22> b;
          b.printf("tcbdump %lx", t->dbg_id());
          delim();
          Jdb::execute_command_long(b.c_str());
        }
    }
  delim("Done");

  if (gzip)
    {
      Kconsole::console()->end_exclusive(Console::GZIP);
      printf("Decode output with: uudecode -o - logfile | zcat\n");
    }

  Jdb_screen::set_height(orig_height);
  Jdb_screen::set_width(orig_width);

  return NOTHING;
}

PUBLIC
int
Jdb_report::num_cmds() const override
{ return 1; }

PUBLIC
Jdb_module::Cmd const *
Jdb_report::cmds() const override
{
  static Cmd cs[] =
    {
      { 0, "Y", "report", "%C", "Y{c}|report <compressed>\tsystem report", &subcmd }
    };
  return cs;
}
