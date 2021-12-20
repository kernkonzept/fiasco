IMPLEMENTATION [mp && pf_exynos && exynos_extgic]:

#include "pic.h"
#include "gic.h"

IMPLEMENT inline
void
Ipi::init(Cpu_number)
{}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_cpu(to_cpu, m);
  stat_sent(from_cpu);
}

PUBLIC static inline
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_bcast(m);
}

