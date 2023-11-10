INTERFACE [arm && pf_ti_am]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_ti_am : Address {
    Gic_dist_phys_base   = 0x01800000,
    Gic_redist_phys_base = 0x01840000,
    Gic_redist_size      =    0xc0000,
  };
};
