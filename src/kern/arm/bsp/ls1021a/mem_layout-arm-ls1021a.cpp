INTERFACE [arm && ls1021a]: //---------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_ls1021a : Address
  {
    Gic_dist_phys_base   = 0x01401000,
    Gic_cpu_phys_base    = 0x01402000,
    Gic_h_phys_base      = 0x01404000,
    Gic_v_phys_base      = 0x01406000,
    Mp_scu_phys_base     = 0,
  };
};
