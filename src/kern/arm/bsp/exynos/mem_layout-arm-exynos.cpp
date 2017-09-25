INTERFACE [arm && exynos4]:

#include <config.h>

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_exynos : Address {
    Sysram_phys_base     = 0x02020000,
    Gpio3_phys_base      = 0x03860000,

    Chip_id_phys_base    = 0x10000000,
    Pmu_phys_base        = 0x10020000,
    Cmu_phys_base        = 0x10030000,
    Mct_phys_base        = 0x10050000,
    Watchdog_phys_base   = 0x10060000,

    Irq_combiner_ext_phys_base   = 0x10440000,
    Irq_combiner_int_phys_base   = 0x10448000,

    Gic_cpu_ext_cpu0_phys_base   = 0x10480000,
    Gic_cpu_ext_cpu1_phys_base   = 0x10488000, // cpu 1 is + 8000
    Gic_dist_ext_cpu0_phys_base  = 0x10490000,
    Gic_dist_ext_cpu1_phys_base  = 0x10498000,

    Devices0_phys_base     = 0x10500000,
    Mp_scu_phys_base       = Devices0_phys_base + 0x00000000,
    Gic_cpu_int_phys_base  = Devices0_phys_base + 0x00000100,
    Gic_dist_int_phys_base = Devices0_phys_base + 0x00001000,
    L2cxx0_phys_base       = Devices0_phys_base + 0x00002000,

    Gpio2_phys_base      = 0x11000000,
    Gpio1_phys_base      = 0x11400000,
    Sromc_phys_base      = 0x12570000,
    Uart_phys_base       = 0x13800000,
    Pwm_phys_base        = 0x139d0000,
  };
};

INTERFACE [arm && exynos4 && exynos_extgic]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_exynos_extgic : Address {
    Gic_cpu_phys_base            = Gic_cpu_ext_cpu0_phys_base,
    Gic_dist_phys_base           = Gic_dist_ext_cpu0_phys_base,
    Irq_combiner_phys_base       = Irq_combiner_ext_phys_base,
  };
};

INTERFACE [arm && exynos4 && !exynos_extgic]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_exynos_intgic : Address {

    Gic_cpu_phys_base            = Gic_cpu_int_phys_base,
    Gic_dist_phys_base           = Gic_dist_int_phys_base,
    Irq_combiner_phys_base       = Irq_combiner_int_phys_base,
  };
};

INTERFACE [arm && exynos5]:

EXTENSION class Mem_layout
{
public:
  enum Phys_layout_exynos : Address {
    Sysram_phys_base       = 0x02020000,
    Gpio3_phys_base        = 0x10d10000,
    Gpio4_phys_base        = 0x03860000,
    Chip_id_phys_base      = 0x10000000,
    Cmu_phys_base          = 0x10010000,
    Pmu_phys_base          = 0x10040000,
    Mct_phys_base          = 0x101c0000,
    Watchdog_phys_base     = 0x101d0000,
    Irq_combiner_phys_base = 0x10440000,
    Gic_dist_phys_base     = 0x10481000,
    Gic_cpu_phys_base      = 0x10482000,
    Gic_h_phys_base        = 0x10484000,
    Gic_v_phys_base        = 0x10486000,

    Mp_scu_phys_base       = 0x10500000,
    Gpio1_phys_base        = 0x11400000,
    Sromc_phys_base        = 0x12250000,
    Uart_phys_base         = 0x12c00000,
    Pwm_phys_base          = 0x12dd0000,
    Gpio2_phys_base        = 0x13400000,
  };
};
