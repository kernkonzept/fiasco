INTERFACE [arm && bcm2835]: //---------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_bcm2835 : Address {
    Timer_phys_base      = 0x20003000,
    Pic_phys_base        = 0x2000b200,
    Watchdog_phys_base   = 0x20100000,
    Uart_phys_base       = 0x20201000,
  };
};
