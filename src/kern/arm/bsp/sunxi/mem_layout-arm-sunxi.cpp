INTERFACE [arm && pf_sunxi]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_sunxi : Address {
    Mp_scu_phys_base     = 0xf8f00000,
    Gic_dist_phys_base   = 0x01c81000,
    Gic_cpu_phys_base    = 0x01c82000,
    Gic_h_phys_base      = 0x01c84000,
    Gic_v_phys_base      = 0x01c86000,
    Timer_phys_base      = 0x01c20c00,
  };
};
