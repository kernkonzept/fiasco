INTERFACE [arm && pf_sr6p7g7]: //----------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address {
    // Map whole GIC
    Gic_phys_base     = 0x6d800000,
    Gic_phys_size     =   0x200000,
    Gic_redist_offset =   0x100000,
  };

  enum Mpu_layout { Mpu_regions = 24 };
};
