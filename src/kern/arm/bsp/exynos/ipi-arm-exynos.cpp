IMPLEMENTATION [mp && exynos && exynos_extgic]:

#include "pic.h"
#include "gic.h"

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_phys_id to_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_cpu(1UL << cxx::int_value<Cpu_phys_id>(to_cpu), m);
  stat_sent(from_cpu);
}

PUBLIC static inline NEEDS["pic.h"]
void Ipi::send(Message m, Cpu_number from_cpu, Cpu_number to_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_cpu(1UL << cxx::int_value<Cpu_phys_id>(_ipi.cpu(to_cpu)._phys_id), m);
  stat_sent(from_cpu);
}

PUBLIC static inline
void
Ipi::bcast(Message m, Cpu_number from_cpu)
{
  Pic::gic.cpu(from_cpu)->softint_bcast(m);
}

