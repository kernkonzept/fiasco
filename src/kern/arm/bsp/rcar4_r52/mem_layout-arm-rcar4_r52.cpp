INTERFACE [arm && pf_rcar4_r52]: //-------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_rcar4_r52: Address {
    // Map whole GIC
    Gic_phys_base     = 0xf0000000,
    Gic_phys_size     =   0x120000,
    Gic_redist_offset =   0x100000,
    Gic_redist_size   =    0x20000,
  };

  enum Mpu_layout {
    Mpu_regions = 24,
  };
};
