INTERFACE:

#include "vgic.h"

struct Gic_h_global
{
  using Arm_vgic = Gic_h::Arm_vgic;
  static Gic_h *gic;
};

IMPLEMENTATION:

Gic_h *Gic_h_global::gic;
