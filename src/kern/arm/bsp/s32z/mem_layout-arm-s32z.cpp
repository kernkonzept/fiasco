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

    Rtu0_Mru0         = 0x76070000,
    Rtu0_Mru1         = 0x76090000,
    Rtu0_Mru2         = 0x76270000,
    Rtu0_Mru3         = 0x76290000,
    Rtu1_Mru0         = 0x76870000,
    Rtu1_Mru1         = 0x76890000,
    Rtu1_Mru2         = 0x76A70000,
    Rtu1_Mru3         = 0x76A90000,
    Mru_size          =    0x1000,
  };

  enum Mpu_layout { Mpu_regions = 20 };
};
