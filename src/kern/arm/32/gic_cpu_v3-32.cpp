IMPLEMENTATION [cpu_virt]:

PRIVATE inline
void Gic_cpu_v3::_enable_sre_set()
{
  asm volatile("mcr p15, 4, %0, c12, c9, 5" // ICC_HSRE
               : : "r" (  ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB
                        | ICC_SRE_Enable_lower));
}

//-------------------------------------------------------------------
IMPLEMENTATION [!cpu_virt]:

PRIVATE inline
void Gic_cpu_v3::_enable_sre_set()
{
  asm volatile("mcr p15, 0, %0, c12, c12, 5" // ICC_SRE
               : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
}

//-------------------------------------------------------------------
IMPLEMENTATION:

PUBLIC inline
void
Gic_cpu_v3::pmr(unsigned prio)
{
  asm volatile("mcr p15, 0, %0, c4, c6, 0" : : "r"(prio));
}

PUBLIC inline NEEDS[Gic_cpu_v3::_enable_sre_set]
void
Gic_cpu_v3::enable()
{
  _enable_sre_set();

  asm volatile("mcr p15, 0, %0, c12, c12, 7" : : "r" (1)); // ICC_IGRPEN1

  pmr(Cpu_prio_val);
}

PUBLIC inline
void
Gic_cpu_v3::ack(Unsigned32 irq)
{
  asm volatile("mcr p15, 0, %0, c12, c12, 1" : : "r"(irq));
}

PUBLIC inline
Unsigned32
Gic_cpu_v3::iar()
{
  Unsigned32 v;
  asm volatile("mrc p15, 0, %0, c12, c12, 0" : "=r"(v));
  return v;
}

PUBLIC inline
unsigned
Gic_cpu_v3::pmr()
{
  Unsigned32 pmr;
  asm volatile("mrc p15, 0, %0, c4, c6, 0" : "=r"(pmr));
  return pmr;
}

PUBLIC inline
void
Gic_cpu_v3::softint(Unsigned64 sgi)
{
  asm volatile("mcrr p15, 0, %Q0, %R0, c12"
               : : "r"(sgi));
}

