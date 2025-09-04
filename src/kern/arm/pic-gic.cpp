//-------------------------------------------------------------------
INTERFACE [arm && pic_gic]:

#include "gic.h"
#include "initcalls.h"
#include "global_data.h"

EXTENSION class Pic
{
public:
  static void init_ap(Cpu_number cpu, bool resume);
  static Global_data<Gic *>gic;
};

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic]:

DEFINE_GLOBAL Global_data<Gic *> Pic::gic;

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && arm_em_tz]:

PUBLIC static
void
Pic::set_pending_irq(unsigned group32num, Unsigned32 val)
{
  gic->set_pending_irq(group32num, val);
}

//-------------------------------------------------------------------
IMPLEMENTATION [arm && pic_gic && mp]:

IMPLEMENT_DEFAULT
void
Pic::init_ap(Cpu_number cpu, bool resume)
{
  gic->init_ap(cpu, resume);
}
