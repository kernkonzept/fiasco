IMPLEMENTATION [cpu_virt]:

PRIVATE inline
void Gic_cpu_v3::_enable_sre_set()
{
  asm volatile("msr S3_4_C12_C9_5, %x0" // ICC_SRE_EL2
               : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
  asm volatile("msr S3_0_C12_C12_5, %x0" // ICC_SRE_EL1
               : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
}

//-------------------------------------------------------------------
IMPLEMENTATION [!cpu_virt]:

PRIVATE inline
void Gic_cpu_v3::_enable_sre_set()
{
  asm volatile("msr S3_0_C12_C12_5, %x0" // ICC_SRE_EL1
               : : "r" (ICC_SRE_SRE | ICC_SRE_DFB | ICC_SRE_DIB));
}


//-------------------------------------------------------------------
IMPLEMENTATION:

#include "mem.h"

PUBLIC inline
void
Gic_cpu_v3::pmr(unsigned prio)
{
  asm volatile("msr S3_0_C4_C6_0, %x0" : : "r" (prio)); // ICC_PMR_EL1
}

PUBLIC inline NEEDS[Gic_cpu_v3::_enable_sre_set, "mem.h"]
void
Gic_cpu_v3::enable()
{
  _enable_sre_set();
  Mem::isb();

  // Make sure ICC_CTLR_EL1.EOImode is set to 0 so that writes to
  // ICC_EOIR1_EL1 also deactivate the interrupt
  asm volatile("msr S3_0_C12_C12_4, %x0" : : "r" (0ul)); // ICC_CTLR_EL1

  asm volatile("msr S3_0_C12_C12_7, %x0" : : "r" (1ul)); // ICC_IGRPEN1_EL1

  pmr(Cpu_prio_val);
}

PUBLIC inline NEEDS["mem.h"]
void
Gic_cpu_v3::disable()
{
  asm volatile("msr S3_0_C12_C12_7, %x0" : : "r" (0ul)); // ICC_IGRPEN1_EL1
  Mem::isb();
}

PUBLIC inline NEEDS["mem.h"]
void
Gic_cpu_v3::ack(Unsigned32 irq)
{
  asm volatile("msr S3_0_C12_C12_1, %x0" : : "r"(irq)); // ICC_EOIR1_EL1
  Mem::isb();
}

PUBLIC inline NEEDS["mem.h"]
Unsigned32
Gic_cpu_v3::iar()
{
  Unsigned32 v;
  asm volatile("mrs %x0, S3_0_C12_C12_0" : "=r"(v)); // ICC_IAR1_EL1
  Mem::dsb();
  return v;
}

PUBLIC inline
unsigned
Gic_cpu_v3::pmr()
{
  Unsigned32 pmr;
  asm volatile("mrs %x0, S3_0_C4_C6_0" : "=r"(pmr)); // ICC_PMR_EL1
  return pmr;
}


PUBLIC inline
void
Gic_cpu_v3::softint(Unsigned64 sgi)
{
  asm volatile("msr S3_0_C12_C11_5, %x0" // ICC_SGI1R_EL1
               : : "r"(sgi));
}

