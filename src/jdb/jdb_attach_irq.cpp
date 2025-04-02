IMPLEMENTATION:

#include <cstdio>

#include "irq_chip.h"
#include "irq.h"
#include "semaphore.h"
#include "irq_mgr.h"
#include "jdb_module.h"
#include "jdb_obj_info.h"
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
Jdb_attach_irq::action( int cmd, void *&args, char const *&, int & ) override
{
  if (cmd)
    return NOTHING;

  if (static_cast<char*>(args) == &subcmd)
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
Jdb_attach_irq::num_cmds() const override
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_attach_irq::cmds() const override
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
  : Jdb_kobject_handler(static_cast<Irq *>(nullptr))
{
  Jdb_kobject::module()->register_handler(this);
}


PUBLIC
bool
Jdb_kobject_irq::handle_key(Kobject_common*, int /* key */) override
{
  return false;
}



PUBLIC
Kobject_common *
Jdb_kobject_irq::follow_link(Kobject_common *o) override
{
  Irq_sender *t = cxx::dyn_cast<Irq_sender*>(o);
  Kobject_common *k = t ? Kobject::from_dbg(Kobject_dbg::pointer_to_obj(t->owner())) : nullptr;
  return k ? k : o;
}

PUBLIC
bool
Jdb_kobject_irq::show_kobject(Kobject_common *, int) override
{ return true; }

PUBLIC
void
Jdb_kobject_irq::show_kobject_short(String_buffer *buf,
                                    Kobject_common *o, bool) override
{
  Irq *i = cxx::dyn_cast<Irq*>(o);
  Kobject_common *w = follow_link(o);

  buf->printf(" I=%3lx %s F=%lx", i->pin(), i->chip()->chip_type(), i->flags());

  if (Irq_sender *t = cxx::dyn_cast<Irq_sender*>(i))
    buf->printf(" L=%lx T=%lx Q=%d",
                t->id(),
                w != o ? w->dbg_info()->dbg_id() : 0,
                t->queued());

  if (Semaphore *t = cxx::dyn_cast<Semaphore*>(i))
    buf->printf(" Q=%ld",
                t->_queued);
}

PUBLIC
bool
Jdb_kobject_irq::info_kobject(Jobj_info *i, Kobject_common *o) override
{
  Irq *irq = cxx::dyn_cast<Irq*>(o);

  if (Irq_sender *t = cxx::dyn_cast<Irq_sender*>(irq))
    {
      i->type = i->irq_sender.Type;
      i->irq_sender.pin = irq->pin();
      strncpy(i->irq_sender.chip_type, irq->chip()->chip_type(),
              sizeof(i->irq_sender.chip_type));
      i->irq_sender.flags = irq->flags();
      i->irq_sender.label = t->id();
      Kobject_common *w = follow_link(o);
      i->irq_sender.target_id = w != o ? w->dbg_info()->dbg_id() : 0;
      i->irq_sender.queued = t->queued();
      return true;
    }
  else if (Semaphore *t = cxx::dyn_cast<Semaphore*>(irq))
    {
      i->type = i->irq_semaphore.Type;
      i->irq_semaphore.pin = irq->pin();
      strncpy(i->irq_semaphore.chip_type, irq->chip()->chip_type(),
              sizeof(i->irq_semaphore.chip_type));
      i->irq_semaphore.flags = irq->flags();
      i->irq_semaphore.queued = t->_queued;
      return true;
    }

  return false;
}

static
bool
filter_irqs(Kobject_common const *o)
{ return cxx::dyn_cast<Irq const *>(o); }

static Jdb_kobject_list::Mode INIT_PRIORITY(JDB_MODULE_INIT_PRIO) tnt("[IRQs]", filter_irqs);

static Jdb_kobject_irq jdb_kobject_irq INIT_PRIORITY(JDB_MODULE_INIT_PRIO);
