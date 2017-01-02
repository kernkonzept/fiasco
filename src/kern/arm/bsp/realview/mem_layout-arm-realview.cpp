INTERFACE [arm && pf_realview]: // ----------------------------------------

#include "globalconfig.h"

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_all : Address {
    Flush_area_phys_base = 0xe0000000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && (pf_realview_eb || pf_realview_pb11mp || pf_realview_pbx || pf_realview_vexpress_a9)]:

#include "globalconfig.h"

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview : Address {
    Devices0_phys_base    = 0x10000000,
    System_regs_phys_base = Devices0_phys_base,
    System_ctrl_phys_base = Devices0_phys_base + 0x00001000,
    Uart_phys_base        = Devices0_phys_base + 0x00009000,
    Timer0_phys_base      = Devices0_phys_base + 0x00011000,
    //Timer1_phys_base      = Devices0_phys_base + 0x00011020,
    //Timer2_phys_base      = Devices0_phys_base + 0x00012000,
    //Timer3_phys_base      = Devices0_phys_base + 0x00012020,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_realview_eb
           && !(arm_mpcore || arm_cortex_a9)]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_single : Address {
    Gic_cpu_phys_base    = Devices0_phys_base  + 0x00040000,
    Gic_dist_phys_base   = Gic_cpu_phys_base   + 0x00001000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_realview_eb
           && (arm_mpcore || arm_cortex_a9)]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_mp : Address {
    Gic1_cpu_phys_base    = Devices0_phys_base + 0x00040000,
    Gic1_dist_phys_base   = Devices0_phys_base + 0x00041000,
#if 0
    Gic2_cpu_phys_base    = Devices0_phys_base + 0x00050000,
    Gic2_dist_phys_base   = Devices0_phys_base + 0x00051000,
    Gic3_cpu_phys_base    = Devices0_phys_base + 0x00060000,
    Gic3_dist_phys_base   = Devices0_phys_base + 0x00061000,
    Gic4_cpu_phys_base    = Devices0_phys_base + 0x00070000,
    Gic4_dist_phys_base   = Devices0_phys_base + 0x00071000,
#endif
    Devices1_phys_base   = 0x10100000,

    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_realview_pb11mp]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_pb11mp : Address {
    Devices1_phys_base   = 0x1f000000,
    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,

    Devices2_phys_base   = 0x1e000000,
    Gic1_cpu_phys_base    = Devices2_phys_base,
    Gic1_dist_phys_base   = Devices2_phys_base + 0x00001000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_realview_pbx]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_pbx : Address {
    Devices1_phys_base    = 0x1f000000,
    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,

    Devices2_phys_base    = 0x1e000000,
    Gic2_cpu_phys_base    = Devices2_phys_base + 0x00020000,
    Gic2_dist_phys_base   = Devices2_phys_base + 0x00021000,
    Gic3_cpu_phys_base    = Devices2_phys_base + 0x00030000,
    Gic3_dist_phys_base   = Devices2_phys_base + 0x00031000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && realview_vexpress_legacy]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_vexpress_a9 : Address {
    Devices1_phys_base   = 0x1e000000,
    Mp_scu_phys_base      = Devices1_phys_base,
    Gic_cpu_phys_base     = Devices1_phys_base + 0x00000100,
    Gic_dist_phys_base    = Devices1_phys_base + 0x00001000,
    L2cxx0_phys_base      = Devices1_phys_base + 0x00002000,
  };
};

// ------------------------------------------------------------------------
INTERFACE [arm && pf_realview_vexpress && !realview_vexpress_legacy]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_realview_vexpress_a15 {
    Devices0_phys_base   = 0x1c000000,
    System_regs_phys_base = 0x1c010000,
    System_ctrl_phys_base = 0x1c020000,
    Uart_phys_base        = 0x1c090000,

    Devices1_phys_base   = 0x1c100000,
    Timer0_phys_base      = Devices1_phys_base + 0x00010000,
    //Timer1_phys_base      = Devices1_phys_base + 0x00010020,
    //Timer2_phys_base      = Devices1_phys_base + 0x00020000,
    //Timer3_phys_base      = Devices1_phys_base + 0x00020020,

    Devices2_phys_base   = 0x2c000000,
    Mp_scu_phys_base      = Devices2_phys_base,
    Gic_cpu_phys_base     = Devices2_phys_base + 0x00002000,
    Gic_dist_phys_base    = Devices2_phys_base + 0x00001000,
    Gic_h_phys_base       = Devices2_phys_base + 0x4000,
    Gic_v_phys_base       = Devices2_phys_base + 0x6000,


    L2cxx0_phys_base      = Devices2_phys_base + 0x00003000,
  };
};
