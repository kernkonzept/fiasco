IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::dtlb_flush(void *va)
{
  asm volatile("mcr p15, 0, %0, c8, c6, 1" // DTLBIMVA
               : : "r" ((unsigned long)va & 0xfffff000) : "memory");
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  asm volatile("mcr p15, 0, %0, c8, c7, 0" // TLBIALL
               : : "r" (0) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long)
{
  asm volatile("mcr p15, 0, %0, c8, c7, 1"
               : : "r" ((unsigned long)va & 0xfffff000) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long)
{
  asm volatile("mcr p15, 0, r0, c8, c7, 0" : : "r" (0) : "memory");
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  btc_flush();
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 0" // TLBIALL
               : : "r" (0) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;
  btc_flush();
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 1" // TLBIMVA
               : : "r" (((unsigned long)va & 0xfffff000) | asid) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 2" // TLBIASID
               : : "r" (asid) : "memory");
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v6plus && cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  btc_flush();
  Mem::dsb();
  asm volatile("mcr p15, 4, r0, c8, c7, 4" : : : "memory"); // TLBIALLNSNH
}

IMPLEMENT inline
void Mem_unit::dtlb_flush(void *va)
{
  asm volatile("mcr p15, 4, %0, c8, c7, 1" // TLBIMVAH
               : : "r" ((unsigned long)va & 0xfffff000) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;
  btc_flush();
  Mem::dsb();
  Mword t1, t2;
  asm volatile(
      "mrrc p15, 6, %[tmp1], %[tmp2], c2 \n"
      "mcrr p15, 6, %[tmp1], %[asid], c2 \n"
      "isb \n"
      "mcr  p15, 0, %[mva], c8, c7, 3 \n"
      "mcrr p15, 6, %[tmp1], %[tmp2], c2 \n"
      : [tmp1] "=&r" (t1), [tmp2] "=&r" (t2)
      : [mva] "r" (((unsigned long)va & 0xfffff000)), [asid] "r" (asid << 16)
      : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsb();
  Mword t1, t2;
  asm volatile(
      "mrrc p15, 6, %[tmp1], %[tmp2], c2 \n"
      "mcrr p15, 6, %[tmp1], %[asid], c2 \n"
      "isb \n"
      "mcr  p15, 0, %[tmp1], c8, c7, 0 \n"
      "mcrr p15, 6, %[tmp1], %[tmp2], c2 \n"
      : [tmp1] "=&r" (t1), [tmp2] "=&r" (t2)
      : [asid] "r" (asid << 16)
      : "memory");
}

IMPLEMENT_OVERRIDE inline
void Mem_unit::kernel_tlb_flush()
{
  asm volatile("mcr p15, 4, r0, c8, c7, 0" : : : "memory"); // TLBIALLH
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v7plus]:

PUBLIC static inline
void
Mem_unit::make_coherent_to_pou(void const *start, size_t size)
{
  unsigned long end = (unsigned long)start + size;
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (auto i = (unsigned long)start & ~(ds - 1U); i < end; i += ds)
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c11, 1" : : "r"(i));  // DCCMVAU

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (auto i = (unsigned long)start & ~(is - 1U); i < end; i += is)
    __asm__ __volatile__ (
        "mcr p15, 0, %0, c7, c5, 1   \n"  // ICIMVAU
        "mcr p15, 0, %0, c7, c5, 7   \n"  // BPIMVA
        : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}
