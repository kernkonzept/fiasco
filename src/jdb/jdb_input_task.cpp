INTERFACE:

#include "jdb_module.h"
#include "jdb_types.h"
#include "types.h"
#include "l4_types.h"


class Task;
class Space;
class Kobject;

class Jdb_input_task_addr
{
public:
  static char     first_char;
  static char     first_char_have_task;
private:
  static Kobject *_task;
  static Space   *_space;
  static Address  _addr;
};


IMPLEMENTATION:

#include <cctype>
#include <cstring>
#include <cstdio>
#include "kernel_console.h"
#include "keycodes.h"
#include "jdb.h"
#include "jdb_kobject.h"
#include "task.h"

char     Jdb_input_task_addr::first_char;
char     Jdb_input_task_addr::first_char_have_task;
Kobject *Jdb_input_task_addr::_task;
Space   *Jdb_input_task_addr::_space;
Address  Jdb_input_task_addr::_addr;


PUBLIC static
Task *
Jdb_input_task_addr::task()
{ return cxx::dyn_cast<Task *>(_task); }

PUBLIC static
Space *
Jdb_input_task_addr::space()
{ return _space; }

PUBLIC static
Address
Jdb_input_task_addr::addr()
{ return _addr; }

PUBLIC static
Jdb_address
Jdb_input_task_addr::address()
{
  if (_task == nullptr && _addr == Invalid_address)
    return Jdb_address::null();

  if (_task == nullptr && _space == nullptr)
    return Jdb_address(_addr); // phys

  if (_space)
    return Jdb_address(_addr, _space); // virt

  // use kmem as last resort
  return Jdb_address::kmem_addr(_addr);
}

PUBLIC static
Jdb_module::Action_code
Jdb_input_task_addr::action(void *&args, char const *&fmt, int &next_char)
{
  if (args == &first_char)
    {
      // initialize
      // so _task is only valid if it is explicitly set
      _task         = nullptr;
      _space        = Jdb::get_space(Jdb::triggered_on_cpu);
      _addr         = Invalid_address;
    }

  if (args == &first_char_have_task)
    first_char = first_char_have_task;

  if (args == &first_char && first_char == 't')
    {
      args = &_task;
      fmt  = strcmp(fmt, " addr=%C") ? " task=%q" : " \b\b\b\b\b\btask=%q";
      return Jdb_module::EXTRA_INPUT;
    }
  else if (args == &_task)
    {
      _space = cxx::dyn_cast<Task *>(_task);

      if (_task && !space())
	{
	  _task = nullptr;
	  puts(" invalid task");
	  return Jdb_module::ERROR;
	}

      args = &first_char_have_task;
      fmt  = " addr=%C";
      return Jdb_module::EXTRA_INPUT;
    }
  else if (first_char == 'p')
    {
      _task = nullptr;
      _space = nullptr;
      putstr(" [phys-mem]");

      args = &first_char_have_task;
      fmt  = " addr=%C";
      return Jdb_module::EXTRA_INPUT;
    }
  else if (args == &first_char || args == &first_char_have_task)
    {
      if (isxdigit(first_char))
	{
	  next_char = first_char;
	  args = &_addr;
	  fmt  = L4_ADDR_INPUT_FMT;
	  return Jdb_module::EXTRA_INPUT_WITH_NEXTCHAR;
	}
      else if (first_char != KEY_RETURN && first_char != KEY_RETURN_2
               && first_char != ' ')
	{
	  fmt = "%C";
	  return Jdb_module::EXTRA_INPUT;
	}
    }

  return Jdb_module::NOTHING;
}


