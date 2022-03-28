INTERFACE [arm && pf_rcar4]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_rcar4 : Address {
    Gic_dist_phys_base   = 0xf1000000,
    Gic_redist_phys_base = 0xf1060000,
    Gic_redist_size      =   0x110000,
  };
};
