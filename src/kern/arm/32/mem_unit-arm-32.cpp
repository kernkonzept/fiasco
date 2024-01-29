//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v5]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 0" // TLBIALL
               : : "r" (0) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long)
{
  tlb_flush();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long)
{
  Mem::dsb();
  asm volatile("mcr p15, 0, %0, c8, c7, 1" // TLBIMVA
               : : "r" ((unsigned long)va & 0xfffff000) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{ tlb_flush(); }

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address va)
{
  // No ASIDs on ARMv5, so just use the regular tlb_flush() implementation
  // passing a dummy ASID value that is ignored anyway.
  tlb_flush((void *)va, 0);
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && (arm_v6 || (arm_v7 && !mp)) && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c7, 0" // TLBIALL
               : : "r" (0) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c7, 2" // TLBIASID
               : : "r" (asid) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c7, 1" // TLBIMVA
               : : "r" (((unsigned long)va & 0xfffff000) | asid) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{ tlb_flush(); }

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address)
{
  // On ARMv6 and ARMv7 without multiprocessing extension, it is not possible to
  // flush all non-global TLB entries for an address without considering the
  // associated ASID, thus perform a full TLB flush.
  tlb_flush_kernel();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && ((arm_v7 && mp) || arm_v8) && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 0" // TLBIALLIS
               : : "r" (0) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 2" // TLBIASIDIS
               : : "r" (asid) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 1" // TLBIMVAIS
               : : "r" (((unsigned long)va & 0xfffff000) | asid) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{ tlb_flush(); }

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("mcr p15, 0, %0, c8, c3, 3" // TLBIMVAAIS
               : : "r" (va & 0xfffff000) : "memory");
  Mem::dsb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v7plus && cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  btc_flush();
  Mem::dsbst();
  asm volatile("mcr p15, 4, r0, c8, c3, 4" : : : "memory"); // TLBIALLNSNHIS
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mword t1, t2;
  asm volatile(
      "mrrc p15, 6, %[tmp1], %[tmp2], c2 \n" // save VTTBR
      "mcrr p15, 6, %[tmp1], %[asid], c2 \n" // write VMID to VTTBR
      "isb \n"
      "dsb ishst \n"
      "mcr  p15, 0, %[tmp1], c8, c3, 0 \n" // TLBIALLIS
      "dsb ish \n"
      "mcrr p15, 6, %[tmp1], %[tmp2], c2 \n" // restore VTTBR
      : [tmp1] "=&r" (t1), [tmp2] "=&r" (t2)
      : [asid] "r" (asid << 16)
      : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;
  btc_flush();
  Mword t1, t2;
  asm volatile(
      "mrrc p15, 6, %[tmp1], %[tmp2], c2 \n" // save VTTBR
      "mcrr p15, 6, %[tmp1], %[asid], c2 \n" // write VMID to VTTBR
      "isb \n"
      "dsb ishst \n"
      "mcr  p15, 0, %[mva], c8, c3, 3 \n" // TLBIMVAAIS
      "dsb ish \n"
      "mcrr p15, 6, %[tmp1], %[tmp2], c2 \n" // restore VTTBR
      : [tmp1] "=&r" (t1), [tmp2] "=&r" (t2)
      : [mva] "r" ((reinterpret_cast<unsigned long>(va) & 0xfffff000)),
        [asid] "r" (asid << 16)
      : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{
  Mem::dsbst();
  asm volatile("mcr p15, 4, r0, c8, c3, 0" : : : "memory"); // TLBIALLHIS
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("mcr p15, 4, %0, c8, c3, 1" // TLBIMVAHIS
               : : "r" (va & 0xfffff000) : "memory");
  Mem::dsb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v7plus]:

PUBLIC static inline
void
Mem_unit::make_coherent_to_pou(void const *start, size_t size)
{
  unsigned long start_addr = reinterpret_cast<unsigned long>(start);
  unsigned long end_addr = start_addr + size;
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (auto i = start_addr & ~(ds - 1U); i < end_addr; i += ds)
    __asm__ __volatile__ ("mcr p15, 0, %0, c7, c11, 1" : : "r"(i));  // DCCMVAU

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (auto i = start_addr & ~(is - 1U); i < end_addr; i += is)
    __asm__ __volatile__ (
        "mcr p15, 0, %0, c7, c5, 1   \n"  // ICIMVAU
        "mcr p15, 0, %0, c7, c5, 7   \n"  // BPIMVA
        : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}
