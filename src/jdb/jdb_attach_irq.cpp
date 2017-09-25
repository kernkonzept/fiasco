IMPLEMENTATION:

#include <cstdio>

#include "irq_chip.h"
#include "irq.h"
#include "semaphore.h"
#include "irq_mgr.h"
#include "jdb_module.h"
#include "kernel_console.h"
#include "static_init.h"
#include "thread.h"
#include "types.h"


//===================
// Std JDB modules
//===================

/**
 * 'IRQ' module.
 *
 * This module handles the 'R' command that
 * provides IRQ attachment and listing functions.
 */
class Jdb_attach_irq : public Jdb_module
{
public:
  Jdb_attach_irq() FIASCO_INIT;
private:
  static char     subcmd;
};

char     Jdb_attach_irq::subcmd;
static Jdb_attach_irq jdb_attach_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

IMPLEMENT
Jdb_attach_irq::Jdb_attach_irq()
  : Jdb_module("INFO")
{}

PUBLIC
Jdb_module::Action_code
Jdb_attach_irq::action( int cmd, void *&args, char const *&, int & )
{
  if (cmd)
    return NOTHING;

  if ((char*)args == &subcmd)
    {
      switch (subcmd)
        {
        case 'l': // list
            {
              Irq_base *r;
              putchar('\n');
	      unsigned n = Irq_mgr::mgr->nr_irqs();
              for (unsigned i = 0; i < n; ++i)
                {
                  r = static_cast<Irq*>(Irq_mgr::mgr->irq(i));
                  if (!r)
                    continue;
                  printf("IRQ %02x/%02u\n", i, i);
                }
              return NOTHING;
            }
        }
    }
  return NOTHING;
}

PUBLIC
int
Jdb_attach_irq::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_attach_irq::cmds() const
{
  static Cmd cs[] =
    {   { 0, "R", "irq", " [l]ist/[a]ttach: %c",
	   "R{l}\tlist IRQ threads", &subcmd }
    };

  return cs;
}


IMPLEMENTATION:

#include "jdb_kobject.h"
#include "irq.h"

class Jdb_kobject_irq : public Jdb_kobject_handler
{
};


PUBLIC inline
Jdb_kobject_irq::Jdb_kobject_irq()
  : Jdb_kobject_handler((Irq*)0)
{
  Jdb_kobject::module()->register_handler(this);
}


PUBLIC
bool
Jdb_kobject_irq::handle_key(Kobject_common *o, int key)
{
  (void)o; (void)key;
  return false;
}



PUBLIC
Kobject_common *
Jdb_kobject_irq::follow_link(Kobject_common *o)
{
  Irq_sender *t = cxx::dyn_cast<Irq_sender*>(o);
  Kobject_common *k = t ? Kobject::from_dbg(Kobject_dbg::pointer_to_obj(t->owner())) : 0;
  return k ? k : o;
}

PUBLIC
bool
Jdb_kobject_irq::show_kobject(Kobject_common *, int)
{ return true; }

PUBLIC
void
Jdb_kobject_irq::show_kobject_short(String_buffer *buf, Kobject_common *o)
{
  Irq *i = cxx::dyn_cast<Irq*>(o);
  Kobject_common *w = follow_link(o);

  buf->printf(" I=%3lx %s F=%x",
              i->pin(), i->chip()->chip_type(),
              (unsigned)i->flags());

  if (Irq_sender *t = cxx::dyn_cast<Irq_sender*>(i))
    buf->printf(" L=%lx T=%lx Q=%d",
                i->obj_id(),
                w != o ?  w->dbg_info()->dbg_id() : 0,
                t ? t->queued() : -1);

  if (Semaphore *t = cxx::dyn_cast<Semaphore*>(i))
    buf->printf(" Q=%ld",
                t->_queued);
}

static
bool
filter_irqs(Kobject_common const *o)
{ return cxx::dyn_cast<Irq const *>(o); }

static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[IRQs]", filter_irqs);

static Jdb_kobject_irq jdb_kobject_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
