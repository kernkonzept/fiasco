IMPLEMENTATION [arm && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  Mem::dsbst();
  asm volatile("tlbi vmalle1is" : : : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  Mem::dsbst();
  asm volatile("tlbi aside1is, %0"
               : : "r" (asid << 48) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;

  Mem::dsbst();
  asm volatile("tlbi vae1is, %0"
               : : "r" ((reinterpret_cast<unsigned long>(va) >> 12)
                        | (asid << 48)) : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{ tlb_flush(); }

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("tlbi vaae1is, %0"
               : : "r" ((va >> 12) & 0x00000ffffffffffful)
               : "memory");
  Mem::dsb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  Mem::dsbst();
  asm volatile("tlbi alle1is" : : : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  Mword vttbr;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[vttbr], vttbr_el2\n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi vmalls12e1is      \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[vttbr]\n"
      :
      [vttbr] "=&r" (vttbr)
      :
      [asid] "r" (asid << 48)
      :
      "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *va, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;

  Mword vttbr;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[vttbr], vttbr_el2\n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi ipas2e1is, %[ipa] \n"
      "dsb ish                \n"
      "tlbi vmalle1is         \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[vttbr]\n"
      :
      [vttbr] "=&r" (vttbr)
      :
      [ipa] "r" (reinterpret_cast<unsigned long>(va) >> 12),
      [asid] "r" (asid << 48)
      :
      "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{
  Mem::dsbst();
  asm volatile("tlbi alle2is" : : : "memory");
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address va)
{
  Mem::dsbst();
  asm volatile("tlbi vae2is, %0"
               : : "r" ((va >> 12) & 0x00000ffffffffffful)
               : "memory");
  Mem::dsb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && arm_v8plus]:

PUBLIC static inline
void
Mem_unit::make_coherent_to_pou(void const *start, size_t size)
{
  unsigned long start_addr = reinterpret_cast<unsigned long>(start);
  unsigned long end_addr = start_addr + size;
  unsigned long is = icache_line_size(), ds = dcache_line_size();

  for (auto i = start_addr & ~(ds - 1U); i < end_addr; i += ds)
    __asm__ __volatile__ ("dc cvau, %0" : : "r"(i));

  Mem::dsb(); // make sure data cache changes are visible to instruction cache

  for (auto i = start_addr & ~(is - 1U); i < end_addr; i += is)
    __asm__ __volatile__ ("ic ivau, %0" : : "r"(i));

  Mem::dsb(); // ensure completion of instruction cache invalidation
}
