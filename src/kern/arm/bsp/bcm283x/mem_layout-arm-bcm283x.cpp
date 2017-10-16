INTERFACE [arm && pf_bcm283x_rpi1]: //-------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm2835 : Address {
    Bcm283x_base = 0x20000000,
  };
};

INTERFACE [arm && (pf_bcm283x_rpi2 || pf_bcm283x_rpi3)]: //----------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm2836_7 : Address {
    Bcm283x_base = 0x3f000000,
  };
};

INTERFACE [arm && pf_bcm283x]: //------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm283x : Address {
    Pic_phys_base        = Bcm283x_base + 0x0000b200,
    Timer_phys_base      = Bcm283x_base + 0x00003000,
    Watchdog_phys_base   = Bcm283x_base + 0x00100000,
  };
};

