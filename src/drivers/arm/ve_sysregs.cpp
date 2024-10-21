// ------------------------------------------------------------------------
INTERFACE [arm && (pf_fvp_base || pf_fvp_base_r)]:

#include "mmio_register_block.h"

class Ve_sysregs : public Mmio_register_block
{
public:
  enum
  {
    // Register offset
    Ve_sysregs_cfgctrl_reg = 0xA4,

    // Register value
    Ve_sysregs_cfgctrl_bit_start = (1 << 31),
    Ve_sysregs_cfgctrl_bit_write = (1 << 30),
    Ve_sysregs_cfgctrl_shift_fn = 20,

    // Functions
    Ve_sysregs_cfgctrl_fn_shutdown = 0x8,
    Ve_sysregs_cfgctrl_fn_reboot   = 0x9,
  };

  Ve_sysregs(void *base) : Mmio_register_block(base) {}
};

// ------------------------------------------------------------------------
IMPLEMENTATION [arm && (pf_fvp_base || pf_fvp_base_r)]:

PRIVATE
void
Ve_sysregs::call(unsigned int fn)
{
  this->r<32>(Ve_sysregs_cfgctrl_reg) = Ve_sysregs_cfgctrl_bit_start
                                        | Ve_sysregs_cfgctrl_bit_write
                                        | (fn << Ve_sysregs_cfgctrl_shift_fn);
}

PUBLIC
void
Ve_sysregs::shutdown()
{
  this->call(Ve_sysregs_cfgctrl_fn_shutdown);
}

PUBLIC
void
Ve_sysregs::reboot()
{
  this->call(Ve_sysregs_cfgctrl_fn_reboot);
}
