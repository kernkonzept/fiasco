INTERFACE [arm && pf_arm_virt]: //---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_virt : Address {
    Mmio_phys_base       = 0x0a000000,
    Gic_dist_phys_base   = 0x08000000,
    Gic_cpu_phys_base    = 0x08010000,
    Gic_h_phys_base      = 0x08200000,
    Uart_phys_base       = 0x09000000,
  };
};

