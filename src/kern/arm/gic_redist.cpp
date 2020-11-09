INTERFACE:

#include "mmio_register_block.h"

class Gic_redist
{
private:
  Mmio_register_block _redist;

public:
  enum
  {
    GICR_CTRL         = 0x0000,
    GICR_IIDR         = 0x0004,
    GICR_TYPER        = 0x0008,
    GICR_STATUSR      = 0x0010,
    GICR_WAKER        = 0x0014,
    GICR_PIDR2        = 0xffe8,
    GICR_SGI_BASE     = 0x10000,
    GICR_IGROUPR0     = GICR_SGI_BASE + 0x0080,
    GICR_ISENABLER0   = GICR_SGI_BASE + 0x0100,
    GICR_ICENABLER0   = GICR_SGI_BASE + 0x0180,
    GICR_ISPENDR0     = GICR_SGI_BASE + 0x0200,
    GICR_ICPENDR0     = GICR_SGI_BASE + 0x0280,
    GICR_ISACTIVER0   = GICR_SGI_BASE + 0x0300,
    GICR_ICACTIVER0   = GICR_SGI_BASE + 0x0380,
    GICR_IPRIORITYR0  = GICR_SGI_BASE + 0x0400,

    GICR_frame_size   = 0x10000,

    GICR_TYPER_VLPIS  = 1 << 1,
    GICR_TYPER_Last   = 1 << 4,

    GICR_WAKER_Processor_sleep = 1 << 1,
    GICR_WAKER_Children_asleep = 1 << 2,
  };

};

// ------------------------------------------------------------------------
IMPLEMENTATION:

#include "cpu.h"
#include "panic.h"
#include "poll_timeout_counter.h"
#include <cstdio>

PUBLIC
void
Gic_redist::find(Address base, Unsigned64 mpidr, Cpu_number cpu)
{
  unsigned o = 0;
  Unsigned64 gicr_typer;
  Unsigned64 typer_aff =   ((mpidr & 0x0000ffffff) << (32 - 0))
                         | ((mpidr & 0xff00000000) << (56 - 32));
  do
    {
      Mmio_register_block r(base + o);

      unsigned id = r.read<Unsigned32>(GICR_PIDR2);
      if (   id != 0x3b    // No GICv3
          && id != 0x4b)   // and no GICv4
        break;

      gicr_typer = r.read<Unsigned64>(GICR_TYPER);
      if ((gicr_typer & 0xffffffff00000000) == typer_aff)
        {
          printf("CPU%d: GIC Redistributor at %lx/%lx for 0x%llx\n",
                 cxx::int_value<Cpu_number>(cpu),
                 (Address)Mem_layout::Gic_redist_phys_base + o,
                 r.get_mmio_base(), mpidr & ~0xc0000000ull);
          _redist = r;
          return;
        }

      o += 2 * GICR_frame_size;
      if (gicr_typer & GICR_TYPER_VLPIS)
        o += 2 * GICR_frame_size;
    }
  while (!(gicr_typer & GICR_TYPER_Last));

  panic("GIC: Did not find a redistributor for CPU%d\n",
        cxx::int_value<Cpu_number>(cpu));
}

PUBLIC
void
Gic_redist::cpu_init()
{
  unsigned val = _redist.read<Unsigned32>(GICR_WAKER);
  if (val & GICR_WAKER_Children_asleep)
    {
      val &= ~GICR_WAKER_Processor_sleep;
      _redist.write<Unsigned32>(val, GICR_WAKER);

      L4::Poll_timeout_counter i(5000000);
      while (i.test(_redist.read<Unsigned32>(GICR_WAKER) & GICR_WAKER_Children_asleep))
        Proc::pause();

      if (i.timed_out())
        panic("GIC: redistributor did not awake\n");
    }

  _redist.write<Unsigned32>(0xffffffff, GICR_ICENABLER0);

  _redist.write<Unsigned32>(0x0000001e, GICR_ISENABLER0);
  _redist.write<Unsigned32>(0xffffffff, GICR_IGROUPR0);

  _redist.write<Unsigned32>(0xffffffff, GICR_ICPENDR0);
  _redist.write<Unsigned32>(0xffffffff, GICR_ICACTIVER0); // clear active

  for (unsigned g = 0; g < 32; g += 4)
    _redist.write<Unsigned32>(0xa0a0a0a0, GICR_IPRIORITYR0 + g);
}

PUBLIC
void
Gic_redist::mask(Mword pin)
{
  _redist.write<Unsigned32>(1u << pin, GICR_ICENABLER0);
}

PUBLIC
void
Gic_redist::unmask(Mword pin)
{
  _redist.write<Unsigned32>(1u << pin, GICR_ISENABLER0);
}

PUBLIC inline
void
Gic_redist::irq_prio(unsigned irq, unsigned prio)
{
  _redist.write<Unsigned8>(prio, GICR_IPRIORITYR0 + irq);
}

PUBLIC inline
unsigned
Gic_redist::irq_prio(unsigned irq)
{
  return _redist.read<Unsigned8>(GICR_IPRIORITYR0 + irq);
}

