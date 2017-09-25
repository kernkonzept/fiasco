INTERFACE [arm && bsp_cpu && (exynos4 || !arm_em_ns)]:

EXTENSION class Scu
{
public:
  enum
  {
    Available   = 1,
    Bsp_enable_bits = Control_scu_standby,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && bsp_cpu && (exynos5 && arm_em_ns)]:

EXTENSION class Scu
{
public:
  enum
  {
    Available = 0,
    Bsp_enable_bits = 0,
  };
};
