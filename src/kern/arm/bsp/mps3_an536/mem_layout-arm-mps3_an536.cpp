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

    // Acutally 32 regions on EL1 and EL2 but we can only handle 31 efficiently
    // at the moment.
    Mpu_regions = 31,
  };
};
