IMPLEMENTATION [arm && exynos && outer_cache_l2cxx0]:

#include "cpu.h"
#include "platform.h"
#include "smc.h"

IMPLEMENT
Mword
Outer_cache::platform_init(Mword auxc)
{
  unsigned tag_lat = 0x110;
  unsigned data_lat = Platform::is_4210() ? 0x110 : 0x120;
  unsigned prefctrl = 0x30000007;
  if (Platform::is_4412() && Platform::subrev() >= 0x10)
    prefctrl = 0x71000007;

  Mword auxc_mask = 0xc200ffff;
  Mword auxc_bits =   (1 <<  0)  // Full Line of Zero Enable
                    | (1 << 16)  // 16 way
                    | (3 << 17)  // way size == 64KB
                    | (1 << 22)  // shared ignored
                    | (3 << 26)  // tz
                    | (1 << 28)  // data prefetch enable
                    | (1 << 29)  // insn prefetch enable
                    | (1 << 30)  // early BRESP enable
                    ;

  if (!Platform::running_ns())
    {
      l2cxx0->write<Mword>(tag_lat, L2cxx0::TAG_RAM_CONTROL);
      l2cxx0->write<Mword>(data_lat, L2cxx0::DATA_RAM_CONTROL);

      l2cxx0->write<Mword>(prefctrl, 0xf60);

      l2cxx0->write<Mword>(L2cxx0::Pwr_ctrl_standby_mode_en
                           | L2cxx0::Pwr_ctrl_dynamic_clk_gating_en,
                           0xf80);
    }
  else
    {
      if (!(l2cxx0->read<Mword>(L2cxx0::CONTROL) & 1))
        invalidate();

      Exynos_smc::l2cache_setup(tag_lat, data_lat, prefctrl,
                                L2cxx0::Pwr_ctrl_standby_mode_en
                                | L2cxx0::Pwr_ctrl_dynamic_clk_gating_en,
                                auxc_bits, auxc_mask);
    }

  return (auxc & auxc_mask) | auxc_bits;
}

IMPLEMENT_OVERRIDE
void
Outer_cache::platform_init_post()
{
  Cpu::enable_cache_foz();
}

