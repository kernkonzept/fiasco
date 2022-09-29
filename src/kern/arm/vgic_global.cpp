INTERFACE:

#include "vgic.h"
#include "per_node_data.h"

struct Gic_h_global
{
  using Arm_vgic = Gic_h::Arm_vgic;
  static Per_node_data<Gic_h *> gic;
};

IMPLEMENTATION:

DECLARE_PER_NODE Per_node_data<Gic_h *> Gic_h_global::gic;
