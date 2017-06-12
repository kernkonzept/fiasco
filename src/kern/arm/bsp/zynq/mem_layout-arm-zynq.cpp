INTERFACE [arm && pf_zynq]: //------------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_zynq : Address {
    Mp_scu_phys_base     = 0xf8f00000,
    Gic_cpu_phys_base    = 0xf8f00100,
    Gic_dist_phys_base   = 0xf8f01000,
    L2cxx0_phys_base     = 0xf8f02000,
  };
};
