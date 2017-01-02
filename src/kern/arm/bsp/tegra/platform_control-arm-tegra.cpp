INTERFACE [arm && mp && pf_tegra]:

#include "mem_layout.h"

EXTENSION class Platform_control
{
private:
  enum
  {
    Reset_vector_addr              = 0x6000f100,
    Clk_rst_ctrl_clk_cpu_cmplx     = 0x6000604c,
    Clk_rst_ctrl_rst_cpu_cmplx_clr = 0x60006344,
    Unhalt_addr                    = 0x60007014,

    CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET = 0x340,
    CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR = 0x344,
  };

  static Mword _orig_reset_vector;
};

INTERFACE [arm && mp && pf_tegra3]:

EXTENSION class Platform_control
{
private:
  enum
  {
    PMC_PWRGATE_TOGGLE          = 0x30,
    PMC_PWRGATE_REMOVE_CLAMPING = 0x34,
    PMC_PWRGATE_STATUS_0        = 0x38,

    PMC_PWRGATE_TOGGLE_START = 1 << 8,

    CLK_RST_CONTROLLER_CLK_CPU_CMPLX_SET_0 = 0x348,
    CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR_0 = 0x34c,
  };
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_tegra2 && !arm_em_ns]:

PRIVATE static
void
Platform_control::init_cpus()
{
  // clocks on other cpu
  Mword r = Io::read<Mword>(Kmem::mmio_remap(Clk_rst_ctrl_clk_cpu_cmplx));
  Io::write<Mword>(r & ~(1 << 9), Kmem::mmio_remap(Clk_rst_ctrl_clk_cpu_cmplx));
  Io::write<Mword>((1 << 13) | (1 << 9) | (1 << 5) | (1 << 1),
		   Kmem::mmio_remap(Clk_rst_ctrl_rst_cpu_cmplx_clr));

  // kick cpu1
  Io::write<Mword>(0, Kmem::mmio_remap(Unhalt_addr));
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_tegra3]:

#include "io.h"
#include "mem.h"
#include "kmem.h"
#include "mmio_register_block.h"
#include "poll_timeout_kclock.h"

PRIVATE static
Mword
Platform_control::pwr_status(int gate)
{
  Mword r = Io::read<Mword>(Kmem::mmio_remap(Kmem::Pmc_phys_base + PMC_PWRGATE_STATUS_0));
  return r & (1 << gate);
}

PRIVATE static
void
Platform_control::delay()
{
  for (int delay = 20000; delay; --delay)
    Mem::barrier();
}

PRIVATE static
void
Platform_control::init_cpus()
{
  int cpu_powergates[4]        = { 0, 9, 10, 11 };
  int flowctrl_cpu_halt_ofs[4] = { 0, 0x14, 0x1c, 0x24 };
  int flowctrl_cpu_csr_ofs[4]  = { 8, 0x18, 0x20, 0x28 };
  Mmio_register_block clk_rst(Kmem::mmio_remap(Mem_layout::Clock_reset_phys_base));
  Mmio_register_block pmc(Kmem::mmio_remap(Mem_layout::Pmc_phys_base));
  Mmio_register_block flow_ctrl(Kmem::mmio_remap(0x60007000));

  for (unsigned i = 1; i < 4; ++i)
    {
      assert(i < 4);
      int gate = cpu_powergates[i];

      // put cpu into reset
      clk_rst.write<Mword>(0x1111 << i, CLK_RST_CONTROLLER_RST_CPU_CMPLX_SET);
      Mem::dmb();

      // flowctrl halt
      flow_ctrl.write<Mword>(0, flowctrl_cpu_halt_ofs[i]);
      flow_ctrl.read<Mword>(flowctrl_cpu_halt_ofs[i]);

      if (!pwr_status(gate))
        {
          pmc.write<Mword>(PMC_PWRGATE_TOGGLE_START | gate, PMC_PWRGATE_TOGGLE);

          Poll_timeout_kclock pt(100000);
          while (pt.test(pwr_status(gate)))
            Proc::pause();

          if (pt.timed_out())
            return;
        }

      // power is on now

      // enable cpu clock
      clk_rst.write<Mword>(1 << (8 + i), CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR_0);
      clk_rst.read<Mword>(CLK_RST_CONTROLLER_CLK_CPU_CMPLX_CLR_0);
      delay();

      // remove clamping
      pmc.write<Mword>(1 << gate, PMC_PWRGATE_REMOVE_CLAMPING);
      delay();

      // clear flow csr
      flow_ctrl.write<Mword>(0, flowctrl_cpu_csr_ofs[i]);
      Mem::wmb();
      flow_ctrl.read<Mword>(flowctrl_cpu_csr_ofs[i]);

      // out of reset
      clk_rst.write<Mword>(0x1111 << i, CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR);
      Mem::wmb();
    }
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_tegra && !arm_em_ns]:

#include "io.h"
#include "kmem.h"

Mword Platform_control::_orig_reset_vector;

PRIVATE static
void Platform_control::reset_orig_reset_vector()
{
  Io::write<Mword>(_orig_reset_vector, Kmem::mmio_remap(Reset_vector_addr));
}

PUBLIC static
void
Platform_control::boot_ap_cpus(Address phys_reset_vector)
{
  // remember original reset vector
  _orig_reset_vector = Io::read<Mword>(Kmem::mmio_remap(Reset_vector_addr));

  // set (temporary) new reset vector
  Io::write<Mword>(phys_reset_vector, Kmem::mmio_remap(Reset_vector_addr));

  //atexit(reset_orig_reset_vector);

  init_cpus();
}

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && mp && pf_tegra && arm_em_ns]:

#include <cstdio>

PUBLIC static
void
Platform_control::boot_ap_cpus(Address)
{
  printf("Platform_control::boot_ap_cpus: Implement.\n");
}
