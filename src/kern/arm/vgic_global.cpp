INTERFACE:

#include "vgic.h"
#include "global_data.h"

struct Gic_h_global
{
  using Arm_vgic = Gic_h::Arm_vgic;
  static Global_data<Gic_h *> gic;
};

IMPLEMENTATION:

DEFINE_GLOBAL Global_data<Gic_h *> Gic_h_global::gic;
