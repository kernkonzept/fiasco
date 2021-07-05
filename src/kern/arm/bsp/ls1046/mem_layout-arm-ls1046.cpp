INTERFACE [arm && pf_ls1046]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_ls1046: Address {
    Gic_dist_phys_base   = 0x01410000,
    Gic_cpu_phys_base    = 0x01420000,
    Gic_h_phys_base      = 0x01440000,
    Gic_v_phys_base      = 0x01460000,
  };
};
