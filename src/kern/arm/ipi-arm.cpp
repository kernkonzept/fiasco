INTERFACE [pic_gic && mp && arm_em_tz]: // --------------------------------

EXTENSION class Ipi
{
public:
  enum { Ipi_start = 8 };
};

// ---------------------------------------------------------------------------
INTERFACE [pic_gic && mp && !arm_em_tz]:

EXTENSION class Ipi
{
public:
  enum { Ipi_start = 1 };
};

// ---------------------------------------------------------------------------
INTERFACE [pic_gic && mp]:

#include "gic.h"

EXTENSION class Ipi
{
public:
  enum Message
  {
    Global_request = Ipi_start, Request, Debug, Timer,
    Ipi_end
  };

private:
  Gic::Sgi_target _sgi_target;
};

// ---------------------------------------------------------------------------
IMPLEMENTATION [pic_gic && mp]:

#include "cpu.h"
#include "pic.h"
#include "processor.h"

PUBLIC inline
Ipi::Ipi() : _sgi_target(~0)
{}

PUBLIC static
void Ipi::ipi_call_debug_arch()
{}

PUBLIC static inline
void Ipi::eoi(Message, Cpu_number on_cpu)
{
  // with the ARM-GIC we have to do the EOI right after the ACK
  stat_received(on_cpu);
}

// ---------------------------------------------------------------------------
IMPLEMENTATION [pic_gic && mp && !irregular_gic]:

IMPLEMENT inline NEEDS["processor.h"]
void
Ipi::init(Cpu_number cpu)
{
  _ipi.cpu(cpu)._sgi_target = Pic::gic->pcpu_to_sgi(Cpu::cpus.cpu(cpu).phys_id());
}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  Pic::gic->softint_cpu(_ipi.cpu(to_cpu)._sgi_target, m);
  stat_sent(from_cpu);
}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_phys_id to_cpu)
{
  Pic::gic->softint_cpu(Pic::gic->pcpu_to_sgi(to_cpu), m);
  stat_sent(from_cpu);
}

PUBLIC static inline
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  (void)from_cpu;
  Pic::gic->softint_bcast(m);
}
