INTERFACE [arm && pf_armada37xx]: //---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_armada37xx : Address
  {
    Gic_dist_phys_base   = 0xd1d00000,
    Gic_redist_phys_base = 0xd1d40000,
    Gic_redist_phys_size = 0x00040000,
  };
};
