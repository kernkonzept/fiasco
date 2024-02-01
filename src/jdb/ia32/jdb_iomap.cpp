IMPLEMENTATION[io]:

#include <cstdio>
#include <cctype>

#include "config.h"
#include "jdb.h"
#include "jdb_module.h"
#include "kmem.h"
#include "simpleio.h"
#include "space.h"
#include "static_init.h"
#include "task.h"

class Jdb_iomap : public Jdb_module
{
public:
  Jdb_iomap() FIASCO_INIT;
private:
  static char first_char;
  static Space *space;
  Address task;
};

char Jdb_iomap::first_char;
Space *Jdb_iomap::space;

static void
Jdb_iomap::show()
{
  Jdb::clear_screen();

  printf("\nIO bitmap for space %p ", static_cast<void *>(space));

  if (space->_bitmap == nullptr)
    {
      // No IO bitmap
      puts("not allocated");
      return;
    }

  printf("at %p\n\nPorts enabled:\n", static_cast<void *>(space->_bitmap));

  bool enabled = false;
  bool any_enabled = false;
  unsigned int count = 0;

  for (unsigned int i = 0; i < Config::Io_port_count; ++i)
    {
      if (space->io_enabled(i) != enabled)
        {
          if (!enabled)
            {
              enabled = true;
              any_enabled = true;
              printf("%04x-", i);
            }
          else
            {
              enabled = false;
              printf("%04x ", i - 1);
            }
        }

      if (enabled)
        ++count;
    }

  if (enabled)
    printf("%04x ", static_cast<unsigned>(Config::Io_port_count - 1));

  if (!any_enabled)
    putstr("<none>");

  printf("\n\nPort counter: %u ", space->_counter);

  if (count == space->_counter)
    puts("(correct)");
  else
    printf("%sshould be %u\033[m\n", Jdb::esc_emph, count);
}

PUBLIC
Jdb_module::Action_code
Jdb_iomap::action(int cmd, void *&args, char const *&fmt, int &next_char) override
{
  if (cmd == 0)
    {
      if (args == &first_char)
        {
          if (isxdigit(first_char))
            {
              fmt = "%q";
              args = &task;
              next_char = first_char;

              return EXTRA_INPUT_WITH_NEXTCHAR;
            }
          else
            task = 0;
        }
      else if (args != &task)
        return NOTHING;

      if (!task)
        return NOTHING;

      space = cxx::dyn_cast<Task *>(reinterpret_cast<Kobject *>(task));
      if (!space)
        return NOTHING;

      show();
    }

  return NOTHING;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_iomap::cmds() const override
{
  static Cmd cs[] =
    {
      {
        0, "r", "iomap", "%C",
        "r[<taskno>]\tdisplay IO bitmap of current/given task",
        &first_char
      },
    };

  return cs;
}

PUBLIC
int
Jdb_iomap::num_cmds() const override
{
  return 1;
}

IMPLEMENT
Jdb_iomap::Jdb_iomap()
  : Jdb_module("INFO")
{}

static Jdb_iomap jdb_iomap INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
