INTERFACE [arm && pf_sbsa]: //---------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_sbsa: Address {
    Gic_its_size = 0x20000,
  };
};
