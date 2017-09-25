INTERFACE [arm-integrator]: //----------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_integrator : Address {
    Integrator_phys_base = 0x10000000,
    Timer_phys_base      = 0x13000000,
    Pic_phys_base        = 0x14000000,
    Uart_phys_base       = 0x16000000,
  };
};

