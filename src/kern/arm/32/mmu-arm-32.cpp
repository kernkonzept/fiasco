IMPLEMENTATION [arm && (arm_v7 || arm_v8)]:

PUBLIC static inline
template< unsigned long Flush_area, bool Ram >
Mword Mmu<Flush_area, Ram>::dcache_line_size()
{
  Mword v;
  __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (v)); // CTR
  return 4 << ((v >> 16) & 0xf);
}

PUBLIC static inline
template< unsigned long Flush_area, bool Ram >
Mword Mmu<Flush_area, Ram>::icache_line_size()
{
  Mword v;
  __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (v)); // CTR
  return 4 << (v & 0xf);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

IMPLEMENT inline
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_cache(void const *start,
				       void const *end)
{
  unsigned long s = reinterpret_cast<unsigned long>(start);
  unsigned long e = reinterpret_cast<unsigned long>(end);
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (unsigned long i = s & ~(ds - 1U); i < e; i += ds)
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c14, 1" : : "r"(i));  // DCCIMVAC

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (unsigned long i = s & ~(is - 1U); i < e; i += is)
    __asm__ __volatile__ (
        "mcr p15, 0, %0, c7, c5, 1   \n"  // ICIMVAU
        "mcr p15, 0, %0, c7, c5, 7   \n"  // BPIMVA
        : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}

IMPLEMENT inline
template< unsigned long Flush_area , bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache(void const *va)
{
  Mem::dsb();
  __asm__ __volatile__ (
      "mcr p15, 0, %0, c7, c10, 1       \n" // DCCMVAC
      :
      : "r" (reinterpret_cast<unsigned long>(va) & ~(dcache_line_size() - 1))
      : "memory");
}

IMPLEMENT inline
template< unsigned long Flush_area , bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache(void const *start, void const *end)
{
  Mem::dsb();
  __asm__ __volatile__ (
      // arm1176 only: "    mcrr p15, 0, %2, %1, c12         \n"
      "1:  mcr p15, 0, %[i], c7, c10, 1   \n" // DCCMVAC
      "    add %[i], %[i], %[clsz]        \n"
      "    cmp %[i], %[end]               \n"
      "    blo 1b                         \n"
      : [i]     "=&r" (start)
      :         "0"   (reinterpret_cast<unsigned long>(start)
                       & ~(dcache_line_size() - 1)),
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
      "1:  mcr p15, 0, %[i], c7, c14, 1 \n" // Clean and Invalidate Data Cache Line (using MVA) Register
      "    add %[i], %[i], %[clsz]      \n"
      "    cmp %[i], %[end]             \n"
      "    blo 1b                       \n"
      : [i]    "=&r" (start)
      :        "0"   (reinterpret_cast<unsigned long>(start)
                      & ~(dcache_line_size() - 1)),
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
      "1:  mcr p15, 0, %[i], c7, c6, 1  \n" // Invalidate Data Cache Line (using MVA) Register
      "    add %[i], %[i], %[clsz]      \n"
      "    cmp %[i], %[end]             \n"
      "    blo 1b                       \n"
      : [i]    "=&r" (start)
      :        "0"   (reinterpret_cast<unsigned long>(start)
                      & ~(dcache_line_size() - 1)),
        [end]  "r"   (end),
	[clsz] "ir"  (dcache_line_size())
      : "memory");
  btc_inv();
  Mem::dsb();
}
//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_mpcore || arm_1136 || arm_1176)]:

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_cache()
{
  Mem::dsb();
  __asm__ __volatile__ (
      "    mcr p15, 0, r0, c7, c14, 0       \n" // Clean and Invalidate Entire Data Cache Register
      "    mcr p15, 0, r0, c7, c5, 0        \n" // Invalidate Entire Instruction Cache Register
      : : : "memory");
  btc_inv();
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::clean_dcache()
{
  Mem::dsb();
  __asm__ __volatile__ (
      "    mcr p15, 0, r0, c7, c10, 0       \n" // Clean Entire Data Cache Register
      : : : "memory");
  btc_inv();
}

IMPLEMENT
template< unsigned long Flush_area, bool Ram >
void Mmu<Flush_area, Ram>::flush_dcache()
{
  Mem::dsb();
  __asm__ __volatile__ (
      "    mcr p15, 0, r0, c7, c14, 0       \n" // Clean and Invalidate Entire Data Cache Register
      : : : "memory");
  btc_inv();
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm]:

EXTENSION class Mmu
{
  static Mword get_clidr()
  {
    Mword clidr;
    asm volatile("mrc p15, 1, %0, c0, c0, 1" : "=r" (clidr)); // CLIDR
    return clidr;
  }

  static Mword get_ccsidr(Mword csselr)
  {
    Mword ccsidr;
    Proc::Status s = Proc::cli_save();
    asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r" (csselr)); // CSSELR
    Mem::isb();
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr)); // CCSIDR
    Proc::sti_restore(s);
    return ccsidr;
  }

  static void dc_cisw(Mword v)
  {
    asm volatile("mcr p15, 0, %0, c7, c14, 2" : : "r" (v) : "memory"); // DCCISW
  }

  static void dc_csw(Mword v)
  {
    asm volatile("mcr p15, 0, %0, c7, c10, 2" : : "r" (v) : "memory"); // DCCSW
  }

  static void dc_isw(Mword v)
  {
    asm volatile("mcr p15, 0, %0, c7, c6, 2" : : "r" (v) : "memory"); // DCISW
  }

  static void ic_iallu()
  {
    asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r" (0) : "memory"); // ICIALLU
  }

};

