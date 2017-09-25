INTERFACE:

#include "irq_chip.h"

/**
 * Internal object to cascade a second IRQ controller on a pin of
 * another IRQ controller.
 */
class Cascade_irq : public Irq_base
{
public:
  explicit Cascade_irq(Irq_chip_icu *cld, Irq_base::Hit_func hit_f)
  : _child(cld)
  {
    set_hit(hit_f);
  }

  Irq_chip_icu *child() const { return _child; }
  void switch_mode(bool) {}

private:
  Irq_chip_icu *_child;
};


