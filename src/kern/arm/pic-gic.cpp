//-------------------------------------------------------------------
INTERFACE [arm && pic_gic]:

#include "gic.h"
#include "initcalls.h"
#include "per_node_data.h"

EXTENSION class Pic
{
public:
  static Per_node_data<Gic *>gic;
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

DECLARE_PER_NODE Per_node_data<Gic *> Pic::gic;

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && arm_em_tz]:

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  (*gic)->set_pending_irq(group32num, val);
}
