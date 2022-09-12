INTERFACE [arm && pf_s32z]: //----------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address
  {
    // Map whole GIC
    Gic_phys_base     = 0x47800000,

    Gic_phys_size     =   0x200000,
    Gic_redist_offset =   0x100000,
    Gic_redist_size   =   0x100000,

    Rtu0_gpr_base     = 0x76120000,
    Rtu1_gpr_base     = 0x76920000,

    Mc_rgm_base       = 0x41850000,
    Mc_me_base        = 0x41900000,
  };

  enum Mpu_layout { Mpu_regions = 20 };
};
