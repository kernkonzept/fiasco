INTERFACE [arm && pf_xscale]: //------------------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_pxa : Address {
    Timer_phys_base      = 0x40a00000,
    Pic_phys_base        = 0x40d00000,
    Flush_area_phys_base = 0xe0000000,
  };
};

