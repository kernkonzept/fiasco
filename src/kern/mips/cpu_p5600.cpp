IMPLEMENTATION:

#include <cpu.h>
namespace {

struct P5600 : Cpu::Hooks
{
  void init(Cpu_number, bool, Unsigned32) override
  {
    // enable FTLB with prio 2
    Mword v;
    asm volatile ("mfc0 %0, $16, 6" : "=r" (v));
    v &= ~(3 << 16);
    v |= (2 << 16) | (1 << 15);
    asm volatile ("mtc0 %0, $16, 6" : : "r" (v));
    asm volatile ("ehb");
  }
};

static P5600 p5600;

DEFINE_MIPS_CPU_TYPE(0x00ffff00, 0x0001a800, &p5600);

}
