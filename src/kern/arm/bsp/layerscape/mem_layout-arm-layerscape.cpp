INTERFACE [arm && pf_layerscape]: //------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_layerscape : Address
  {
    Gic_dist_phys_base   = 0x01401000,
    Gic_cpu_phys_base    = 0x01402000,
    Gic_h_phys_base      = 0x01404000,
    Gic_v_phys_base      = 0x01406000,
  };
};
