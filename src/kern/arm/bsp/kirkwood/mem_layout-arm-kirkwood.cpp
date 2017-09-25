INTERFACE [arm && kirkwood]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_kirkwood: Address
  {
    Reset_phys_base   = 0xf1000000,
    Timer_phys_base   = 0xf1000000,
    Pic_phys_base     = 0xf1000000,
  };
};
