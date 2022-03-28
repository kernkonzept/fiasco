INTERFACE [arm && pf_s32s]: //----------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s32: Address {
    // Map whole GIC
    Gic_phys_base     = 0x50800000,
    Gic_phys_size     =   0x140000,
    Gic_redist_offset =   0x100000,
    Gic_redist_size   =    0x20000,

    S32s_r52_cluster  = 0x4007c400,
    S32_mc_me         = 0x40088000,
    S32_mc_rgm        = 0x40078000,
  };
};

//------------------------------------------------------------------
INTERFACE [arm && pf_s32s && cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Mpu_layout {
    Mpu_regions = 16,
  };
};

//------------------------------------------------------------------
INTERFACE [arm && pf_s32s && !cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Mpu_layout {
    Mpu_regions = 20,
  };
};
