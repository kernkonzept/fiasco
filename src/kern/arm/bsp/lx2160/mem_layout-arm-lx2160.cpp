INTERFACE [arm && pf_lx2160]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_lx2160: Address {
    Gic_dist_phys_base   = 0x06000000,
    Gic_redist_phys_base = 0x06200000,
    Gic_redist_size      =   0x200000,
    Gic_h_phys_base      = 0x0c0d0000,
    Gic_v_phys_base      = 0x0c0e0000,
  };
};
