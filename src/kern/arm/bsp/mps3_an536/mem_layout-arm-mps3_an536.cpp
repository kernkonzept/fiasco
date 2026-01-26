INTERFACE [arm && pf_mps3_an536]: //------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_mps3_an536 : Address {
    // Map whole GIC
    Gic_phys_base     = 0xf0000000,
    Gic_phys_size     =   0x200000,
    Gic_redist_offset =   0x100000,
    Gic_redist_size   =   0x100000,
  };

  enum Mpu_layout { Mpu_regions = 24 };
};
