INTERFACE [arm && pf_s3c2410]: //----------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_s3c2410 : Address {
    Pic_phys_base        = 0x4a000000,
    Uart_phys_base       = 0x50000000,
    Pwm_phys_base        = 0x51000000,
    Watchdog_phys_base   = 0x53000000,
  };
};
