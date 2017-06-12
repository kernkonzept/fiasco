INTERFACE [arm && pf_rcar3]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_rcar3 : Address {
    Gic_cpu_phys_base    = 0xf1020000,
    Gic_dist_phys_base   = 0xf1010000,
    Gic_h_phys_base      = 0xf1040000,
    Gic_v_phys_base      = 0xf1060000,
  };
};
