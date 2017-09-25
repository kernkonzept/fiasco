/**
 * @brief Jdb-Utcb module
 *
 * This module shows the user tcbs and the vCPU state of a thread/vcpu
 */

IMPLEMENTATION:

#include <cassert>
#include <cstdio>
#include "l4_types.h"
#include "config.h"
#include "jdb.h"
#include "jdb_kobject.h"
#include "jdb_module.h"
#include "space.h"
#include "static_init.h"
#include "thread_object.h"
#include "thread_state.h"

class Jdb_utcb : public Jdb_module
{
public:
  Jdb_utcb() FIASCO_INIT;
private:
  static Kobject *thread;
};


static Jdb_utcb Jdb_utcb INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

Kobject *Jdb_utcb::thread;

IMPLEMENT
Jdb_utcb::Jdb_utcb()
  : Jdb_module("INFO")
{}

PUBLIC static
void
Jdb_utcb::print(Thread *t)
{
  if (t->utcb().kern())
    {
      printf("\nUtcb-addr: %p\n", t->utcb().kern());
      t->utcb().kern()->print();
    }

  if (t->state(false) & Thread_vcpu_enabled)
    {
      Vcpu_state *v = t->vcpu_state().kern();
      printf("\nVcpu-state-addr: %p\n", v);
      printf("state: %x    saved-state:  %x  sticky: %x\n",
             (unsigned)v->state, (unsigned)v->_saved_state,
             (unsigned)v->sticky_flags);
      printf("entry_sp = %lx    entry_ip = %lx  sp = %lx\n",
             v->_entry_sp, v->_entry_ip, v->_sp);
      v->_regs.dump();
    }
}

PUBLIC virtual
Jdb_module::Action_code
Jdb_utcb::action( int cmd, void *&, char const *&, int &)
{
  if (cmd)
    return NOTHING;

  Thread *t = cxx::dyn_cast<Thread *>(thread);
  if (!t)
    {
      printf(" Invalid thread\n");
      return NOTHING;
    }

  print(t);

  return NOTHING;
}

PUBLIC
int
Jdb_utcb::num_cmds() const
{ return 1; }

PUBLIC
Jdb_module::Cmd
const * Jdb_utcb::cmds() const
{
  static Cmd cs[] =
    {
      { 0, "z", "z", "%q", "z<thread>\tshow UTCB and vCPU state", &thread }
    };
  return cs;
}

// --------------------------------------------------------------------------
// Handler for kobject list

class Jdb_kobject_utcb_hdl : public Jdb_kobject_handler
{
public:
  virtual bool show_kobject(Kobject_common *, int) { return true; }
  virtual ~Jdb_kobject_utcb_hdl() {}
};

PUBLIC static FIASCO_INIT
void
Jdb_kobject_utcb_hdl::init()
{
  static Jdb_kobject_utcb_hdl hdl;
  Jdb_kobject::module()->register_handler(&hdl);
}

PUBLIC
bool
Jdb_kobject_utcb_hdl::handle_key(Kobject_common *o, int keycode)
{
  if (keycode == 'z')
    {
      Thread *t = cxx::dyn_cast<Thread *>(o);
      if (!t)
        return false;

      Jdb_utcb::print(t);
      Jdb::getchar();
      return true;
    }

  return false;
}

STATIC_INITIALIZE(Jdb_kobject_utcb_hdl);
