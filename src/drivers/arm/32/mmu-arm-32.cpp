IMPLEMENTATION [arm && (arm_v7 || arm_v8)]:

PUBLIC static inline
Mword Mmu::dcache_line_size()
{
  Mword v;
  __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (v)); // CTR
  return 4 << ((v >> 16) & 0xf);
}

PUBLIC static inline
Mword Mmu::icache_line_size()
{
  Mword v;
  __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 1" : "=r" (v)); // CTR
  return 4 << (v & 0xf);
}

//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus]:

IMPLEMENT inline ALWAYS_INLINE
void Mmu::flush_cache(void const *start, void const *end)
{
  unsigned long ds = dcache_line_size();
  auto s = reinterpret_cast<unsigned long>(start) & ~(ds - 1U);
  auto e = (reinterpret_cast<unsigned long>(end) + ds - 1U) & ~(ds - 1);

  for (unsigned long i = s; i != e; i += ds)
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c14, 1" : : "r"(i));  // DCCIMVAC

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  unsigned long is = icache_line_size();
  s = reinterpret_cast<unsigned long>(start) & ~(is - 1U);
  e = (reinterpret_cast<unsigned long>(end) + is - 1U) & ~(is - 1U);

  for (unsigned long i = s; i != e; i += is)
    __asm__ __volatile__ (
        "mcr p15, 0, %0, c7, c5, 1   \n"  // ICIMVAU
        "mcr p15, 0, %0, c7, c5, 7   \n"  // BPIMVA
        : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}

IMPLEMENT inline
void Mmu::clean_dcache(void const *va)
{
  __asm__ __volatile__ (
      "mcr p15, 0, %0, c7, c10, 1       \n" // DCCMVAC
      :
      : "r" (reinterpret_cast<unsigned long>(va) & ~(dcache_line_size() - 1))
      : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mmu::clean_dcache(void const *start, void const *end)
{
  unsigned long ds = dcache_line_size();
  unsigned long s = reinterpret_cast<unsigned long>(start);
  unsigned long e = reinterpret_cast<unsigned long>(end);

  __asm__ __volatile__ (
      // arm1176 only: "    mcrr p15, 0, %2, %1, c12         \n"
      "1:  mcr p15, 0, %[i], c7, c10, 1   \n" // DCCMVAC
      "    add %[i], %[i], %[clsz]        \n"
      "    cmp %[i], %[end]               \n"
      "    bne 1b                         \n"
      : [i]     "=&r" (start)
      :         "0"   (s & ~(ds - 1)),
        [end]   "r"   ((e + ds - 1) & ~(ds - 1)),
	[clsz]  "ir"  (ds)
      : "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT
void Mmu::flush_dcache(void const *start, void const *end)
{
  unsigned long ds = dcache_line_size();
  unsigned long s = reinterpret_cast<unsigned long>(start);
  unsigned long e = reinterpret_cast<unsigned long>(end);

  __asm__ __volatile__ (
      "1:  mcr p15, 0, %[i], c7, c14, 1 \n" // Clean and Invalidate Data Cache Line (using MVA) Register
      "    add %[i], %[i], %[clsz]      \n"
      "    cmp %[i], %[end]             \n"
      "    bne 1b                       \n"
      : [i]    "=&r" (start)
      :        "0"   (s & ~(ds - 1)),
        [end]  "r"   ((e + ds - 1) & ~(ds - 1)),
	[clsz] "ir"  (ds)
      : "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT
void Mmu::inv_dcache(void const *start, void const *end)
{
  unsigned long ds = dcache_line_size();
  unsigned long s = reinterpret_cast<unsigned long>(start);
  unsigned long e = reinterpret_cast<unsigned long>(end);

  __asm__ __volatile__ (
      "1:  mcr p15, 0, %[i], c7, c6, 1  \n" // Invalidate Data Cache Line (using MVA) Register
      "    add %[i], %[i], %[clsz]      \n"
      "    cmp %[i], %[end]             \n"
      "    bne 1b                       \n"
      : [i]    "=&r" (start)
      :        "0"   (s & ~(ds - 1)),
        [end]  "r"   ((e + ds - 1) & ~(ds - 1)),
	[clsz] "ir"  (ds)
      : "memory");
  btc_inv();
  Mem::dsb();
}
//-----------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_mpcore || arm_1136 || arm_1176)]:

IMPLEMENT inline ALWAYS_INLINE
void Mmu::flush_cache()
{
  __asm__ __volatile__ (
      "    mcr p15, 0, r0, c7, c14, 0       \n" // Clean and Invalidate Entire Data Cache Register
      "    mcr p15, 0, r0, c7, c5, 0        \n" // Invalidate Entire Instruction Cache Register
      : : : "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT
void Mmu::clean_dcache()
{
  __asm__ __volatile__ (
      "    mcr p15, 0, r0, c7, c10, 0       \n" // Clean Entire Data Cache Register
      : : : "memory");
  btc_inv();
  Mem::dsb();
}

IMPLEMENT
void Mmu::flush_dcache()
{
  __asm__ __volatile__ (
      "    mcr p15, 0, r0, c7, c14, 0       \n" // Clean and Invalidate Entire Data Cache Register
      : : : "memory");
  btc_inv();
  Mem::dsb();
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

  static Unsigned64 get_ccsidr(Mword csselr)
  {
    Mword ccsidr;
    Mword ccsidr2 = 0;
    Proc::Status s = Proc::cli_save();
    asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r" (csselr)); // CSSELR
    Mem::isb();
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r" (ccsidr)); // CCSIDR
    if (has_feat_ccidx())
      asm("mrc p15, 1, %0, c0, c0, 2" : "=r" (ccsidr2)); // CCSIDR2
    Proc::sti_restore(s);
    return ccsidr | (static_cast<Unsigned64>(ccsidr2) << 32);
  }

  static bool has_feat_ccidx()
  {
    if constexpr (!TAG_ENABLED(arm_v8plus))
      return false;

    Mword f;
    asm("mrc p15, 0, %0, c0, c2, 6": "=r" (f)); // ID_MMFR4
    return (f >> 24) & 0xf;
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

