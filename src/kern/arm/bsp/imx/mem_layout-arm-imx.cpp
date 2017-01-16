INTERFACE [arm && pf_imx]: //----------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx : Address {
    Flush_area_phys_base = 0xe0000000,
  };
};

INTERFACE [arm && pf_imx_21]: // ---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx21 : Address {
    Timer_phys_base       = 0x10003000,
    Pll_phys_base         = 0x10027000,
    Watchdog_phys_base    = 0x10002000,
    Pic_phys_base         = 0x10040000,
  };
};

INTERFACE [arm && pf_imx_28]: // ---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx28 : Address {
    Timer_phys_base       = 0x80068000,
    Pic_phys_base         = 0x80000000,
  };
};

INTERFACE [arm && pf_imx_35]: // ---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx35 : Address {
    Timer_phys_base       = 0x53f94000, // epit1
    Watchdog_phys_base    = 0x53fdc000, // wdog
    Pic_phys_base         = 0x68000000,
  };
};


INTERFACE [arm && pf_imx_51]: // ---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx51 : Address {
    Timer_phys_base       = 0x73fac000, // epit1
    Watchdog_phys_base    = 0x73f98000, // wdog1
    Gic_dist_phys_base    = 0xe0000000,
    Gic_cpu_phys_base     = 0xe0000000, // this is a fake address and not used
  };
};

INTERFACE [arm && pf_imx_53]: // ---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx53 : Address {
    Timer_phys_base       = 0x53fac000, // epit1
    Watchdog_phys_base    = 0x53f98000, // wdog1
    Gic_dist_phys_base    = 0x0fffc000,
    Gic_cpu_phys_base     = 0x0fffc000, // this is a fake address and not used
  };
};

INTERFACE [arm && pf_imx_6]: // -----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx6 : Address {
    Timer_phys_base      = 0x020d0000,
    Mp_scu_phys_base     = 0x00a00000,
    Gic_cpu_phys_base    = 0x00a00100,
    Gic_dist_phys_base   = 0x00a01000,
    L2cxx0_phys_base     = 0x00a02000,

    Watchdog_phys_base   = 0x020bc000, // wdog1
    Gpt_phys_base        = 0x02098000,
    Src_phys_base        = 0x020d8000,
  };
};

INTERFACE [arm && pf_imx_6ul]: // ---------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx6ul : Address {
    Gic_dist_phys_base   = 0x00a01000,
    Gic_cpu_phys_base    = 0x00a02000,
    Gic_h_phys_base      = 0x00a04000,
    Gic_v_phys_base      = 0x00a06000,

    Watchdog_phys_base   = 0x020bc000, // wdog1
  };
};

INTERFACE [arm && pf_imx_7]: // -----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_imx7 : Address {
    Gic_dist_phys_base   = 0x31001000,
    Gic_cpu_phys_base    = 0x31002000,
    Gic_h_phys_base      = 0x31004000,
    Gic_v_phys_base      = 0x31006000,

    Watchdog_phys_base   = 0x30280000, // wdog2
    Src_phys_base        = 0x30390000,
    Gpc_phys_base        = 0x303a0000,
  };
};
