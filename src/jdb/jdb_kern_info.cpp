INTERFACE:

#include "jdb_module.h"
#include <cxx/slist>

class Jdb_kern_info_module;

/**
 * 'kern info' module.
 *
 * This module handles the 'k' command, which
 * prints out various kernel information.
 */
class Jdb_kern_info : public Jdb_module
{
public:
  Jdb_kern_info() FIASCO_INIT;
private:
  typedef cxx::S_list_bss<Jdb_kern_info_module> Module_list;
  typedef Module_list::Iterator Module_iter;
  static char                 _subcmd;
  static Module_list modules;
};


class Jdb_kern_info_module : public cxx::S_list_item
{
  friend class Jdb_kern_info;
public:
  Jdb_kern_info_module(char subcmd, char const *descr) FIASCO_INIT;
private:
  virtual void show(void) = 0;
  char                 _subcmd;
  char const           *_descr;
};


IMPLEMENTATION:

#include <cctype>
#include <cstdio>

#include "cpu.h"
#include "jdb.h"
#include "jdb_module.h"
#include "static_init.h"
#include "kmem_alloc.h"


//===================
// Std JDB modules
//===================


IMPLEMENT
Jdb_kern_info_module::Jdb_kern_info_module(char subcmd, char const *descr)
{
  _subcmd = subcmd;
  _descr  = descr;
}


char Jdb_kern_info::_subcmd;
Jdb_kern_info::Module_list Jdb_kern_info::modules;

PUBLIC static
void
Jdb_kern_info::register_subcmd(Jdb_kern_info_module *m)
{
  Module_iter p;
  for (p = modules.begin();
       p != modules.end()
       && (tolower(p->_subcmd) < tolower(m->_subcmd)
           || (tolower(p->_subcmd) == tolower(m->_subcmd)
               && p->_subcmd > m->_subcmd));
      ++p)
    ;

  modules.insert_before(m, p);
}

PUBLIC
Jdb_module::Action_code
Jdb_kern_info::action(int cmd, void *&args, char const *&, int &)
{
  if (cmd != 0)
    return NOTHING;

  char c = *(char*)(args);
  Module_iter kim;

  for (kim = modules.begin(); kim != modules.end(); ++kim)
    {
      if (kim->_subcmd == c)
	{
	  putchar('\n');
	  kim->show();
	  putchar('\n');
	  return NOTHING;
	}
    }

  putchar('\n');
  for (kim = modules.begin(); kim != modules.end(); ++kim)
    printf("  k%c   %s\n", kim->_subcmd, kim->_descr);

  putchar('\n');
  return NOTHING;
}

PUBLIC
int
Jdb_kern_info::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_kern_info::cmds() const
{
  static Cmd cs[] =
    {
	{ 0, "k", "k", "%c",
	   "k\tshow various kernel information (kh=help)", &_subcmd }
    };

  return cs;
}

IMPLEMENT
Jdb_kern_info::Jdb_kern_info()
  : Jdb_module("INFO")
{}

static Jdb_kern_info jdb_kern_info INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
