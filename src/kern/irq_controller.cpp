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
Icu::icu_get_irq(unsigned irqnum)
{
  return Irq_mgr::mgr->irq(irqnum);
}


PUBLIC inline NEEDS["irq_mgr.h"]
L4_msg_tag
Icu::op_icu_bind(unsigned irqnum, Ko::Cap<Irq> const &irq)
{
  if (!Ko::check_rights(irq.rights, Ko::Rights::CW()))
    return commit_result(-L4_err::EPerm);

  auto g = lock_guard(irq.obj->irq_lock());
  irq.obj->unbind();

  if (!Irq_mgr::mgr->alloc(irq.obj, irqnum))
    return commit_result(-L4_err::EPerm);

  return commit_result(0);
}

PUBLIC inline NEEDS["irq_mgr.h"]
L4_msg_tag
Icu::op_icu_set_mode(Mword pin, Irq_chip::Mode mode)
{
  Irq_mgr::Irq i = Irq_mgr::mgr->chip(pin);

  if (!i.chip)
    return commit_result(-L4_err::ENodev);

  int r = i.chip->set_mode(i.pin, mode);

  Irq_base *irq = i.chip->irq(i.pin);
  if (irq)
    {
      auto g = lock_guard(irq->irq_lock());
      if (irq->chip() == i.chip && irq->pin() == i.pin)
        irq->switch_mode(i.chip->is_edge_triggered(i.pin));
    }

  return commit_result(r);
}


PUBLIC inline NEEDS["irq_mgr.h"]
L4_msg_tag
Icu::op_icu_get_info(Mword *features, Mword *num_irqs, Mword *num_msis)
{
  *num_irqs = Irq_mgr::mgr->nr_irqs();
  *num_msis = Irq_mgr::mgr->nr_msis();
  *features = *num_msis ? (unsigned)Msi_bit : 0;
  return L4_msg_tag(0);
}

PUBLIC inline NEEDS["irq_mgr.h"]
L4_msg_tag
Icu::op_icu_msi_info(Mword msi, Unsigned64 source, Irq_mgr::Msi_info *out)
{
  return commit_result(Irq_mgr::mgr->msg(msi, source, out));
}


PUBLIC inline
Icu::Icu()
{
  initial_kobjects.register_obj(this, Initial_kobjects::Icu);
}

