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
  Mword tmp;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[tmp], vttbr_el2  \n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi ipas2e1, %[ipa]   \n"
      "dsb ish                \n"
      "tlbi vmalle1           \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[tmp]  \n"
      :
      [tmp] "=&r" (tmp)
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
  Mword tmp;
  // FIXME: could do a compare for the current VMID before loading
  // the vttbr and the isb
  asm volatile(
      "mrs %[tmp], vttbr_el2  \n"
      "msr vttbr_el2, %[asid] \n"
      "isb                    \n"
      "dsb ishst              \n"
      "tlbi vmalls12e1        \n"
      "dsb ish                \n"
      "msr vttbr_el2, %[tmp]  \n"
      :
      [tmp] "=&r" (tmp)
      :
      [asid] "r" (asid << 48)
      :
      "memory");
}
