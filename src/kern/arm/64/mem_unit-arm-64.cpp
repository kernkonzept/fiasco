IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  asm volatile("tlbi vmalle1" : : : "memory");
}

IMPLEMENT inline
void Mem_unit::dtlb_flush(void *va)
{
  asm volatile("tlbi vae1, %0"
               : : "r" ((((unsigned long)va) >> 12) & 0x00000ffffffffffful)
               : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;

  Mem::dsb();
  asm volatile("tlbi vae1, %0"
               : : "r" (((unsigned long)va >> 12)
                        | (asid << 48)) : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsb();
  asm volatile("tlbi aside1, %0" // TLBIASID
               : : "r" (asid << 48) : "memory");
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

IMPLEMENT_OVERRIDE inline
void Mem_unit::kernel_tlb_flush()
{
  asm volatile("dsb ishst; tlbi alle2; dsb ish" : : : "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  asm volatile("dsb ishst; tlbi alle1; dsb ish" : : : "memory");
}

IMPLEMENT inline
void Mem_unit::dtlb_flush(void *va)
{
  asm volatile("dsb ishst; tlbi vae2, %0"
               : : "r" ((((unsigned long)va) >> 12) & 0x00000ffffffffffful)
               : "memory");
}


IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;

  Mem::dsb();
  Mword vttbr;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[vttbr], vttbr_el2\n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi ipas2e1, %[ipa]   \n"
      "dsb ish                \n"
      "tlbi vmalle1           \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[vttbr]\n"
      :
      [vttbr] "=&r" (vttbr)
      :
      [ipa] "r" ((unsigned long)va >> 12),
      [asid] "r" (asid << 48)
      :
      "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsb();
  Mword vttbr;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[vttbr], vttbr_el2\n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi vmalls12e1        \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[vttbr]\n"
      :
      [vttbr] "=&r" (vttbr)
      :
      [asid] "r" (asid << 48)
      :
      "memory");
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8plus]:

PUBLIC static inline
void
Mem_unit::make_coherent_to_pou(void const *start, size_t size)
{
  unsigned long end = (unsigned long)start + size;
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (auto i = (unsigned long)start & ~(ds - 1U); i < end; i += ds)
    __asm__ __volatile__ ("dc cvau, %0" : : "r"(i));

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (auto i = (unsigned long)start & ~(is - 1U); i < end; i += is)
    __asm__ __volatile__ ("ic ivau, %0" : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}
