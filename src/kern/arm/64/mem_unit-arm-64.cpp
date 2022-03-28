IMPLEMENTATION [arm && mmu && !cpu_virt]:

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
IMPLEMENTATION [arm && mmu && cpu_virt]:

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
IMPLEMENTATION [arm && !mmu && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{}

IMPLEMENT inline
void Mem_unit::dtlb_flush(void *)
{}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *, unsigned long)
{
  Mem::dsb();
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long)
{
  btc_flush();
  Mem::dsb();
}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu && cpu_virt]:

/*
 * ARM DDI 0600A.c D1.6.2:
 * Stage 1 VMSAv8-64 is permitted to cache stage 2 PMSAv8-64 MPU configuration
 * as a part of the translation process. Visibility of stage 2 MPU updates for
 * stage 1 VMSAv8-64 contexts requires associated TLB invalidation for stage 2.
 *
 * => We still need to do TLB maintenance.
 *
 * FIXME: revise what instructions are really necessary here.
 */

IMPLEMENT_OVERRIDE inline
void Mem_unit::kernel_tlb_flush()
{}

IMPLEMENT inline
void Mem_unit::tlb_flush()
{
  asm volatile("dsb ishst; tlbi alle1; dsb ish" : : : "memory");
}

IMPLEMENT inline
void Mem_unit::dtlb_flush(void * /*va*/)
{}

IMPLEMENT inline
void Mem_unit::tlb_flush(void * /*va*/, unsigned long asid)
{
  if (asid == Asid_invalid)
    return;

  tlb_flush(asid);
}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long asid)
{
  btc_flush();
  Mem::dsb();

  Mword tmp;
  asm volatile(
      "mrs %[tmp], S3_4_c2_c0_0   \n" // VSCTLR_EL2
      "msr S3_4_c2_c0_0, %[asid]  \n"
      "isb                        \n"
      "dsb ishst                  \n"
      "tlbi vmalls12e1            \n"
      "dsb ish                    \n"
      "msr S3_4_c2_c0_0, %[tmp]   \n"
      :
      [tmp] "=&r" (tmp)
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
