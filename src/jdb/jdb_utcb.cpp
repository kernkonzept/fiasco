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
Jdb_utcb::print(Thread *t, bool overlayprint)
{
  char const *clreol_lf = "\n";
  if (overlayprint)
    {
      clreol_lf = Jdb::clear_to_eol_lf_str();
      Jdb::line();
    }

  if (t->utcb().kern())
    {
      printf("\nUtcb-addr: %p%s", t->utcb().kern(), clreol_lf);
      t->utcb().kern()->print(clreol_lf);
    }

  if (t->state(false) & Thread_vcpu_enabled)
    {
      Vcpu_state *v = t->vcpu_state().kern();
      printf("%sVcpu-state-addr: %p%s", clreol_lf, v, clreol_lf);
      printf("state: %x    saved-state:  %x  sticky: %x%s",
             (unsigned)v->state, (unsigned)v->_saved_state,
             (unsigned)v->sticky_flags, clreol_lf);
      printf("entry_sp = %lx    entry_ip = %lx  sp = %lx%s",
             v->_entry_sp, v->_entry_ip, v->_sp, clreol_lf);
      v->_regs.dump();
    }

  if (overlayprint)
    Jdb::line();
}

PUBLIC virtual
Jdb_module::Action_code
Jdb_utcb::action( int cmd, void *&, char const *&, int &) override
{
  if (cmd)
    return NOTHING;

  Thread *t = cxx::dyn_cast<Thread *>(thread);
  if (!t)
    {
      printf(" Invalid thread\n");
      return NOTHING;
    }

  print(t, false);

  return NOTHING;
}

PUBLIC
int
Jdb_utcb::num_cmds() const override
{ return 1; }

PUBLIC
Jdb_module::Cmd
const * Jdb_utcb::cmds() const override
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
  bool show_kobject(Kobject_common *, int) override
  { return true; }

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
Jdb_kobject_utcb_hdl::handle_key(Kobject_common *o, int keycode) override
{
  if (keycode == 'z')
    {
      Thread *t = cxx::dyn_cast<Thread *>(o);
      if (!t)
        return false;

      Jdb_utcb::print(t, true);
      Jdb::getchar();
      return true;
    }

  return false;
}

PUBLIC
char const *
Jdb_kobject_utcb_hdl::help_text(Kobject_common *o) const override
{
  if (cxx::dyn_cast<Thread *>(o))
    return "z=UTCB/vCPU";

  return 0;
}

STATIC_INITIALIZE(Jdb_kobject_utcb_hdl);
