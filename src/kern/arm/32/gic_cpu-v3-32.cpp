IMPLEMENTATION [cpu_virt]:

PRIVATE inline
void Gic_cpu::_enable_sre_set()
{
  asm volatile("mcr p15, 4, %0, c12, c9, 5" // ICC_HSRE
               : : "r" (  ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB
                        | ICC_SRE_Enable_lower));
}

//-------------------------------------------------------------------
IMPLEMENTATION [!cpu_virt]:

PRIVATE inline
void Gic_cpu::_enable_sre_set()
{
  asm volatile("mcr p15, 0, %0, c12, c12, 5" // ICC_SRE
               : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
}

//-------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
void
Gic_cpu::pmr(unsigned prio)
{
  asm volatile("mcr p15, 0, %0, c4, c6, 0" : : "r"(prio));
}

PUBLIC inline NEEDS[Gic_cpu::_enable_sre_set]
void
Gic_cpu::enable()
{
  _enable_sre_set();

  asm volatile("mcr p15, 0, %0, c12, c12, 7" : : "r" (1)); // ICC_IGRPEN1

  pmr(Cpu_prio_val);
}

PUBLIC inline
void
Gic_cpu::ack(Unsigned32 irq)
{
  asm volatile("mcr p15, 0, %0, c12, c12, 1" : : "r"(irq));
}

PUBLIC inline
Unsigned32
Gic_cpu::iar()
{
  Unsigned32 v;
  asm volatile("mrc p15, 0, %0, c12, c12, 0" : "=r"(v));
  return v;
}

PUBLIC inline
unsigned
Gic_cpu::pmr()
{
  Unsigned32 pmr;
  asm volatile("mrc p15, 0, %0, c4, c6, 0" : "=r"(pmr));
  return pmr;
}

PUBLIC inline
void
Gic_cpu::softint_cpu(Unsigned64 sgi_target, unsigned m)
{
  asm volatile("mcrr p15, 0, %Q0, %R0, c12"
               : : "r"(sgi_target | (m << 24)));
}

PUBLIC inline
void
Gic_cpu::softint_bcast(unsigned m)
{
  asm volatile("mcrr p15, 0, %Q0, %R0, c12"
               : : "r"((1ull << 40) | (m << 24)));
}

PUBLIC static inline
Unsigned64
Gic_cpu::pcpu_to_sgi(Cpu_phys_id cpu)
{
  Mword mpidr = cxx::int_value<Cpu_phys_id>(cpu);

  Unsigned8 aff0 = (mpidr >>  0) & 0xff;
  Unsigned8 aff1 = (mpidr >>  8) & 0xff;
  Unsigned8 aff2 = (mpidr >> 16) & 0xff;

  return ((Unsigned64)aff2 << 32) | (aff1 << 16) | (1u << aff0);
}


