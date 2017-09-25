INTERFACE [arm && bsp_cpu && omap5]:

EXTENSION class Scu
{
public:
  enum
    {
      Available = 0,
      Bsp_enable_bits = 0,
    };
};
