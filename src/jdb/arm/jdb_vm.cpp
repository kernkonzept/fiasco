IMPLEMENTATION [arm_em_tz]:

#include <cstring>
#include <cstdio>

#include "jdb_module.h"
#include "jdb_kobject.h"
#include "jdb_obj_info.h"
#include "static_init.h"
#include "vm.h"

class Jdb_vm : public Jdb_kobject_handler
{
public:
  Jdb_vm() FIASCO_INIT;
};

IMPLEMENT
Jdb_vm::Jdb_vm()
  : Jdb_kobject_handler(static_cast<VM *>(nullptr))
{
  Jdb_kobject::module()->register_handler(this);
}

PUBLIC
bool
Jdb_vm::show_kobject(Kobject_common *o, int lvl)
{
  cxx::dyn_cast<Vm *>(o)->dump_vm_state();
  if (lvl)
    {
      Jdb::getchar();
      return true;
    }

  return false;
}

PUBLIC
char const *
Jdb_vm::kobject_type(Kobject_common *) const
{
  return JDB_ANSI_COLOR(yellow) "Vm" JDB_ANSI_COLOR(default);
}

PUBLIC
void
Jdb_vm::show_kobject_short(String_buffer *buf, Kobject_common *o, bool) override
{
  return cxx::dyn_cast<Vm *>(o)->show_short(buf);
}

PUBLIC
bool
Jdb_vm::info_kobject(Jobj_info *i, Kobject_common *o) override
{
  i->type = Jobj_info::Type_vm;
  return cxx::dyn_cast<Vm *>(o)->info_kobject(i, o);
}

static Jdb_vm jdb_vm INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

static
bool
filter_vm(Kobject_common const *o)
{
  return cxx::dyn_cast<Vm const *>(o);
}
static Jdb_kobject_list::Mode
       INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[Vms]", filter_vm);
