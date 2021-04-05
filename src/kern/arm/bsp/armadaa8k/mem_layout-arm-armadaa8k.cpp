INTERFACE [arm && pf_armadaa8k]: //----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_armadaa8k : Address
  {
    Gic_dist_phys_base   = 0xf0210000,
    Gic_cpu_phys_base    = 0xf0220000,
    Gic_h_phys_base      = 0xf0240000,
    Gic_v_phys_base      = 0xf0260000,
  };
};
