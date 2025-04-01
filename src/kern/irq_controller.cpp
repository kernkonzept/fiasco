INTERFACE:

#include "irq.h"
#include "ram_quota.h"
#include "icu_helper.h"

class Irq_chip;

class Icu : public Icu_h<Icu>
{
  friend class Icu_h<Icu>;
};

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include "entry_frame.h"
#include "irq.h"
#include "irq_chip.h"
#include "irq_mgr.h"
#include "l4_types.h"
#include "l4_buf_iter.h"

PUBLIC void
Icu::operator delete (void *)
{
  printf("WARNING: tried to delete kernel ICU object.\n"
         "         The system is now useless\n");
}

PUBLIC inline NEEDS["irq_mgr.h"]
Irq_base *
Icu::icu_irq(unsigned irqnum)
{
  return Irq_mgr::mgr->irq(irqnum);
}

PUBLIC inline NEEDS["irq_mgr.h"]
Irq_mgr::Irq
Icu::icu_get_chip(Mword pin) const
{
  return Irq_mgr::mgr->chip(pin);
}

PUBLIC inline NEEDS["irq_mgr.h"]
int
Icu::icu_attach(unsigned irqnum, Irq_base *irq)
{
  if (Irq_mgr::mgr->attach(irq, irqnum))
    return 0;

  return -L4_err::EInval;
}


PUBLIC inline NEEDS["irq_mgr.h"]
int
Icu::icu_get_info(Mword *features, Mword *num_irqs, Mword *num_msis)
{
  *num_irqs = Irq_mgr::mgr->nr_irqs();
  *num_msis = Irq_mgr::mgr->nr_msis();
  *features = *num_msis ? Mword{Msi_bit} : 0;
  return 0;
}

PUBLIC inline NEEDS["irq_mgr.h"]
int
Icu::icu_msi_info(Mword msi, Unsigned64 source, Irq_mgr::Msi_info *out)
{
  return Irq_mgr::mgr->msg(msi, source, out);
}

PUBLIC inline
Icu::Icu()
{
  initial_kobjects->register_obj(this, Initial_kobjects::Icu);
}

