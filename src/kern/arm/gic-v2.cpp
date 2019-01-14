INTERFACE:

EXTENSION class Gic
{
public:
  typedef Unsigned32 Sgi_target;
};

//-------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
Gic::Sgi_target
Gic::pcpu_to_sgi(Cpu_phys_id cpu)
{ return Gic_dist::pcpu_to_sgi(cpu); }

PUBLIC inline
void
Gic::softint_cpu(Gic::Sgi_target callmap, unsigned m)
{ _dist.softint_cpu(callmap, m); }

PUBLIC inline
void
Gic::softint_bcast(unsigned m)
{ _dist.softint_bcast(m); }

PRIVATE inline
void
Gic::cpu_local_init(Cpu_number)
{
  _dist.cpu_init();
}

PRIVATE inline void Gic::check_sgi_mapping()
{
  // Ensure BSPs have provided a mapping for the CPUTargetList
  assert(pcpu_to_sgi(Proc::cpu_id()) < (1 << 8));
}

//-------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC
void
Gic::irq_prio_bootcpu(unsigned irq, unsigned prio)
{
  _dist.irq_prio(irq, prio);
}

PUBLIC
unsigned
Gic::irq_prio_bootcpu(unsigned irq)
{
  return _dist.irq_prio(irq);
}
