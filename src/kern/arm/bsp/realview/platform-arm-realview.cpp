INTERFACE[arm && pf_realview]:

#include "mem_layout.h"
#include "mmio_register_block.h"

class Platform
{
public:
  class Sys : public Mmio_register_block
  {
  public:
    enum Registers
    {
      Id        = 0x0,
      Sw        = 0x4,
      Led       = 0x8,
      Lock      = 0x20,
      Flags     = 0x30,
      Flags_clr = 0x34,
      Reset     = 0x40,
      Cnt_24mhz = 0x5c,
      Pld_ctrl1 = 0x74,
      Pld_ctrl2 = 0x78,
    };
    explicit Sys(Address virt) : Mmio_register_block(virt) {}
  };

  class System_control : public Mmio_register_block
  {
  public:
    enum
    {
      Timer0_enable = 1UL << 15,
      Timer1_enable = 1UL << 17,
      Timer2_enable = 1UL << 19,
      Timer3_enable = 1UL << 21,
    };
    explicit System_control(Address virt) : Mmio_register_block(virt) {}
  };

  static Static_object<Sys> sys;
  static Static_object<System_control> system_control;
};


IMPLEMENTATION[arm && pf_realview]:

#include "kmem.h"
#include "static_init.h"


Static_object<Platform::Sys> Platform::sys;
// hmmm
Static_object<Platform::System_control> Platform::system_control;
static void platform_init()
{
  if (Platform::sys->get_mmio_base())
    return;

  Platform::sys.construct(Kmem::mmio_remap(Mem_layout::System_regs_phys_base));
  Platform::system_control.construct(Kmem::mmio_remap(Mem_layout::System_ctrl_phys_base));
}

STATIC_INITIALIZER_P(platform_init, ROOT_FACTORY_INIT_PRIO);
