IMPLEMENTATION:

#include <cpu.h>
namespace {

struct P5600 : Cpu::Hooks
{
  void init(Cpu_number, bool, Unsigned32) override
  {
    using namespace Mips;
    // enable FTLB
    Mword v = mfc0_32(Cp0_config_6);
    v &= ~(3 << 16);
    v |= (2 << 16) | (1 << 15);
    mtc0_32(v, Cp0_config_6);
    ehb();
  }
};

static P5600 p5600;

DEFINE_MIPS_CPU_TYPE(0x00ffff00, 0x0001a800, &p5600);

}
