INTERFACE [arm && pf_omap3_35x]: //----------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_omap3_35x : Address {
    Wkup_cm_phys_base        = 0x48004c00,
    L4_addr_prot_phys_base   = 0x48040000,
    Uart1_phys_base          = 0x4806a000,
    Gptimer10_phys_base      = 0x48086000,

    Intc_phys_base           = 0x48200000,

    Prm_global_reg_phys_base = 0x48307200,
    Timer1ms_phys_base       = 0x48318000,

    Uart3_phys_base          = 0x49020000,
  };
};

INTERFACE [arm && pf_omap3_omap35xevm]: //----------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_omap3_35xevm : Address {
    Uart_phys_base           = Uart1_phys_base,
  };
};

INTERFACE [arm && pf_omap3_beagleboard]: //--------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_omap3_beagleboard : Address {
    Uart_phys_base           = Uart3_phys_base,
  };
};

INTERFACE [arm && pf_omap3_am33xx]: //-------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_omap3_335x : Address {
    Cm_per_phys_base         = 0x44e00000,
    Cm_wkup_phys_base        = 0x44e00400,
    Cm_dpll_phys_base        = 0x44e00500,
    Timergen_phys_base       = 0x44e05000, // DMTIMER0
    Timer1ms_phys_base       = 0x44e31000,
    Uart1_phys_base          = 0x44e09000,
    Prm_global_reg_phys_base = 0x48107200,
    Intc_phys_base           = 0x48200000,
    Uart_phys_base           = Uart1_phys_base,
  };
};

INTERFACE [arm && pf_omap4]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_omap4_pandaboard : Address {
    Uart_phys_base          = 0x48020000,
    Mp_scu_phys_base        = 0x48240000,
    Gic_cpu_phys_base       = 0x48240100,
    Gic_dist_phys_base      = 0x48241000,
    L2cxx0_phys_base        = 0x48242000,

    __Timer                 = 0x48240600,

    Prm_phys_base           = 0x4a306000,
  };
};

INTERFACE [arm && pf_omap5]: //--------------------------------------------

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_omap5 : Address {
    Uart_phys_base          = 0x48020000,
    Gic_dist_phys_base      = 0x48211000,
    Gic_cpu_phys_base       = 0x48212000,
    Gic_h_phys_base         = 0x48214000,
    Gic_v_phys_base         = 0x48216000,

    Prm_phys_base           = 0x4ae06000,
  };
};
