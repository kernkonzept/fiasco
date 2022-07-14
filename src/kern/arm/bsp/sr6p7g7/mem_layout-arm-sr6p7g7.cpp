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
};

//------------------------------------------------------------------
INTERFACE [arm && pf_sr6p7g7 && cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Mpu_layout {
    Mpu_regions = 24,
  };
};

//------------------------------------------------------------------
INTERFACE [arm && pf_sr6p7g7 && !cpu_virt]:

EXTENSION class Mem_layout
{
public:
  enum Mpu_layout {
    Mpu_regions = 24,
  };
};
