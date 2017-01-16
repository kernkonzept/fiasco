//-------------------------------------------------------------------
INTERFACE [arm && pic_gic]:

#include "gic.h"
#include "initcalls.h"

EXTENSION class Pic
{
public:
  static Static_object<Gic> gic;
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

Static_object<Gic> Pic::gic;

extern "C"
void irq_handler()
{ Pic::gic->hit(0); }

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && arm_em_tz]:

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  gic->set_pending_irq(group32num, val);
}
