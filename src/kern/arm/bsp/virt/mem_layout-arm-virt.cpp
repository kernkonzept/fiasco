INTERFACE [arm && pf_arm_virt]: //---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_virt : Address
  {
    Gic_dist_phys_base   = 0x08000000,
    Gic_cpu_phys_base    = 0x08010000,
    Gic_h_phys_base      = 0x08030000,
    Gic_v_phys_base      = 0x08040000,
    Gic_redist_phys_base = 0x080A0000,
    Gic_redist_phys_size = 0x00F60000,
  };
};
