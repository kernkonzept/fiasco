IMPLEMENTATION [arm && arm_v8]:

PUBLIC static inline
template< unsigned long Flush_area, bool Ram >
Mword Mmu<Flush_area, Ram>::dcache_line_size()
{
  Mword v;
  __asm__ __volatile__("msr CSSELR_EL1, %1; mrs %0, CCSIDR_EL1" : "=r" (v) : "r"(0));
  return 16 << (v & 0x7);
}

PUBLIC static inline
template< unsigned long Flush_area, bool Ram >
Mword Mmu<Flush_area, Ram>::icache_line_size()
{
  Mword v;
  __asm__ __volatile__("msr CSSELR_EL1, %1; mrs %0, CCSIDR_EL1" : "=r" (v) : "r"(1));
  return 16 << (v & 0x7);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

IMPLEMENT inline
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_cache(void const *start,
				       void const *end)
{
  unsigned i = icache_line_size(), d = dcache_line_size();
  __asm__ __volatile__ (
      "1:  dc civac, %[i]  \n"
      "    ic ivau,  %[i]  \n"
      "    add %[i], %[i], %[clsz]       \n"
      "    cmp %[i], %[end]              \n"
      "    blo 1b                        \n"
      : [i]     "=&r" (start)
      :         "0"   ((unsigned long)start & ~((i < d ? d : i) - 1)),
        [end]   "r"   (end),
	[clsz]  "ir"  (i < d ? i : d)
      : "r0", "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT inline
template< unsigned long Flush_area , bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache(void const *va)
{
  Mem::dsb();
  __asm__ __volatile__ (
      "dc cvac, %0       \n" // DCCMVAC
      :
      : "r" ((unsigned long)va & ~(dcache_line_size() - 1))
      : "memory");
}

IMPLEMENT inline
template< unsigned long Flush_area , bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache(void const *start, void const *end)
{
  Mem::dsb();
  __asm__ __volatile__ (
      "1:  dc cvac, %[i]  \n"
      "    add %[i], %[i], %[clsz]        \n"
      "    cmp %[i], %[end]               \n"
      "    blo 1b                         \n"
      : [i]     "=&r" (start)
      :         "0"   ((unsigned long)start & ~(dcache_line_size() - 1)),
        [end]   "r"   (end),
	[clsz]  "ir"  (dcache_line_size())
      : "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_dcache(void const *start, void const *end)
{
  Mem::dsb();
  __asm__ __volatile__ (
      "1:  dc civac, %[i] \n"
      "    add %[i], %[i], %[clsz]      \n"
      "    cmp %[i], %[end]             \n"
      "    blo 1b                       \n"
      : [i]    "=&r" (start)
      :        "0"   ((unsigned long)start & ~(dcache_line_size() - 1)),
        [end]  "r"   (end),
	[clsz] "ir"  (dcache_line_size())
      : "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::inv_dcache(void const *start, void const *end)
{
  Mem::dsb();
  __asm__ __volatile__ (
      "1:  dc ivac, %[i] \n"
      "    add %[i], %[i], %[clsz]      \n"
      "    cmp %[i], %[end]             \n"
      "    blo 1b                       \n"
      : [i]    "=&r" (start)
      :        "0"   ((unsigned long)start & ~(dcache_line_size() - 1)),
        [end]  "r"   (end),
	[clsz] "ir"  (dcache_line_size())
      : "memory");
  btc_inv();
  Mem::dsb();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8]:

EXTENSION class Mmu
{
  static Mword get_clidr()
  {
    Mword clidr;
    asm volatile("mrs %0, CLIDR_EL1" : "=r" (clidr));
    return clidr;
  }

  static Mword get_ccsidr(Mword csselr)
  {
    Mword ccsidr;
    Proc::Status s = Proc::cli_save();
    asm volatile("msr CSSELR_EL1, %0" : : "r" (csselr));
    Mem::isb();
    asm volatile("mrs %0, CCSIDR_EL1" : "=r" (ccsidr));
    Proc::sti_restore(s);
    return ccsidr;
  }

  static void dc_cisw(Mword v)
  {
    asm volatile("dc cisw, %0" : : "r" (v) : "memory");
  }

  static void dc_csw(Mword v)
  {
    asm volatile("dc csw, %0" : : "r" (v) : "memory");
  }

  static void ic_iallu()
  {
    asm volatile("ic iallu" : : : "memory");
  }
};

