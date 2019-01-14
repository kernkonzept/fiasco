IMPLEMENTATION [cpu_virt]:

PRIVATE inline
void Gic_cpu::_enable_sre_set()
{
  asm volatile("msr S3_4_C12_C9_5, %0" // ICC_SRE_EL2
               : : "r" (  ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB
                        | ICC_SRE_Enable_lower));
}

//-------------------------------------------------------------------
IMPLEMENTATION [!cpu_virt]:

PRIVATE inline
void Gic_cpu::_enable_sre_set()
{
  asm volatile("msr S3_0_C12_C12_5, %0" // ICC_SRE_EL1
               : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
}


//-------------------------------------------------------------------
IMPLEMENTATION:

#include "mem_unit.h"

PUBLIC inline
void
Gic_cpu::pmr(unsigned prio)
{
  asm volatile("msr S3_0_C4_C6_0, %0" : : "r" (prio)); // ICC_PMR_EL1
}

PUBLIC inline NEEDS[Gic_cpu::_enable_sre_set, "mem_unit.h"]
void
Gic_cpu::enable()
{
  _enable_sre_set();
  Mem::isb();

  asm volatile("msr S3_0_C12_C12_7, %0" : : "r" (1)); // ICC_IGRPEN1_EL1

  pmr(Cpu_prio_val);
}

PUBLIC inline NEEDS["mem_unit.h"]
void
Gic_cpu::ack(Unsigned32 irq)
{
  asm volatile("msr S3_0_C12_C12_1, %0" : : "r"(irq)); // ICC_EOIR1_EL1
  Mem::isb();
}

PUBLIC inline NEEDS["mem_unit.h"]
Unsigned32
Gic_cpu::iar()
{
  Unsigned32 v;
  asm volatile("mrs %0, S3_0_C12_C12_0" : "=r"(v)); // ICC_IAR1_EL1
  Mem::dsb();
  return v;
}

PUBLIC inline
unsigned
Gic_cpu::pmr()
{
  Unsigned32 pmr;
  asm volatile("mrs %0, S3_0_C4_C6_0" : "=r"(pmr)); // ICC_PMR_EL1
  return pmr;
}


PUBLIC inline
void
Gic_cpu::softint_cpu(Unsigned64 sgi_target, unsigned m)
{
  asm volatile("msr S3_0_C12_C11_5, %0" // ICC_SGI1R_EL1
               : : "r"(sgi_target | (m << 24)));
}

PUBLIC inline
void
Gic_cpu::softint_bcast(unsigned m)
{
  asm volatile("msr S3_0_C12_C11_5, %0" // ICC_SGI1R_EL1
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
  Unsigned8 aff3 = (mpidr >> 32) & 0xff;

  return    ((Unsigned64)aff3 << 48)
          | ((Unsigned64)aff2 << 32)
          | ((Unsigned64)aff1 << 16)
          | (1u << aff0);
}

