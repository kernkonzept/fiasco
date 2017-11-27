INTERFACE [arm && pf_zynqmp]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_zynqmp: Address {
    Gic_cpu_phys_base    = 0xf9020000,
    Gic_dist_phys_base   = 0xf9010000,
    Gic_h_phys_base      = 0xf9040000,
    Gic_v_phys_base      = 0xf9060000,
  };
};
