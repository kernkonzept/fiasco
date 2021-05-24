INTERFACE [arm && pf_fvp_base]: // ----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_fvp {
    Gic_dist_phys_base   = 0x2f000000,
    Gic_redist_phys_base = 0x2f100000,
    Gic_redist_size      = 0x00200000,
  };
};
