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
};

// ---------------------------------------------------------------------------
IMPLEMENTATION [pic_gic && mp]:

#include "cpu.h"
#include "pic.h"

PUBLIC inline
Ipi::Ipi()
{}

PUBLIC static inline
void Ipi::eoi(Message, Cpu_number on_cpu)
{
  // with the ARM-GIC we have to do the EOI right after the ACK
  stat_received(on_cpu);
}

// ---------------------------------------------------------------------------
IMPLEMENTATION [pic_gic && mp && !irregular_gic]:

IMPLEMENT inline
void
Ipi::init(Cpu_number)
{
}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  Pic::gic->softint_cpu(to_cpu, m);
  stat_sent(from_cpu);
}
PUBLIC static inline NEEDS["pic.h"]
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  (void)from_cpu;
  Pic::gic->softint_bcast(m);
}
