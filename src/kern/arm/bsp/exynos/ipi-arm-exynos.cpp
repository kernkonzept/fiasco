IMPLEMENTATION [mp && pf_exynos && exynos_extgic]:

#include "pic.h"
#include "gic.h"

IMPLEMENT inline NEEDS["processor.h"]
void
Ipi::init(Cpu_number cpu)
{
  _ipi.cpu(cpu)._sgi_target = Pic::gic.cpu(cpu)->pcpu_to_sgi(Cpu::cpus.cpu(cpu).phys_id());
}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_phys_id to_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_cpu(1UL << Pic::gic.cpu(from_cpu)->pcpu_to_sgi(to_cpu), m);
  stat_sent(from_cpu);
}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_cpu(1UL << _ipi.cpu(to_cpu)._sgi_target, m);
  stat_sent(from_cpu);
}

PUBLIC static inline
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_bcast(m);
}

