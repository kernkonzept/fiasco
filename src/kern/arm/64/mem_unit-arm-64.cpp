IMPLEMENTATION [arm && mmu && !cpu_virt]:

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
IMPLEMENTATION [arm && mmu && cpu_virt]:

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
IMPLEMENTATION [arm && !mmu && !cpu_virt]:

IMPLEMENT inline
void Mem_unit::tlb_flush()
{}

IMPLEMENT inline
void Mem_unit::tlb_flush(unsigned long)
{}

IMPLEMENT inline
void Mem_unit::tlb_flush(void *, unsigned long)
{}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address)
{}

//---------------------------------------------------------------------------
IMPLEMENTATION [arm && !mmu && cpu_virt]:

/*
 * ARM DDI 0600A.c D1.6.2:
 * Stage 1 VMSAv8-64 is permitted to cache stage 2 PMSAv8-64 MPU configuration
 * as a part of the translation process. Visibility of stage 2 MPU updates for
 * stage 1 VMSAv8-64 contexts requires associated TLB invalidation for stage 2.
 *
 * => We still need to do TLB maintenance.
 */

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
  Mword vsctlr;
  asm volatile(
      "   mrs %[vsctlr], S3_4_c2_c0_0 \n" // VSCTLR_EL2
      "   cmp %[vsctlr], %[asid]      \n"
      "   dsb ishst                   \n"
      "   b.eq 1f                     \n"

      // Flush TLB for differnet task -> briefly switch VMID.
      "   msr S3_4_c2_c0_0, %[asid]   \n" // VSCTLR_EL2
      "   isb                         \n"
      "   tlbi vmalls12e1is           \n"
      "   dsb ish                     \n"
      "   msr S3_4_c2_c0_0, %[vsctlr] \n" // VSCTLR_EL2
      "   b 2f                        \n"

      // Current task -> no need to switch VMID. But it might have been
      // switched by a previous tlb_flush(). Make sure potential VSCTLR_EL2
      // writes have retired.
      "1: isb                         \n"
      "   tlbi vmalls12e1is           \n"
      "   dsb ish                     \n"

      // done
      "2:                             \n"
      :
      [vsctlr] "=&r" (vsctlr)
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

  Mword vsctlr;
  asm volatile(
      "   mrs %[vsctlr], S3_4_c2_c0_0 \n" // VSCTLR_EL2
      "   cmp %[vsctlr], %[asid]      \n"
      "   dsb ishst                   \n"
      "   b.eq 1f                     \n"

      // Flush TLB for differnet task -> briefly switch VMID.
      "   msr S3_4_c2_c0_0, %[asid]   \n" // VSCTLR_EL2
      "   isb                         \n"
      "   tlbi ipas2e1is, %[ipa]      \n"
      "   dsb ish                     \n"
      "   tlbi vmalle1is              \n"
      "   dsb ish                     \n"
      "   msr S3_4_c2_c0_0, %[vsctlr] \n" // VSCTLR_EL2
      "   b 2f                        \n"

      // Current task -> no need to switch VMID. But it might have been
      // switched by a previous tlb_flush(). Make sure potential VSCTLR_EL2
      // writes have retired.
      "1: isb                         \n"
      "   tlbi ipas2e1is, %[ipa]      \n"
      "   dsb ish                     \n"
      "   tlbi vmalle1is              \n"
      "   dsb ish                     \n"

      // done
      "2:                             \n"
      :
      [vsctlr] "=&r" (vsctlr)
      :
      [ipa] "r" (reinterpret_cast<unsigned long>(va) >> 12),
      [asid] "r" (asid << 48)
      :
      "memory");
}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel()
{}

IMPLEMENT inline
void Mem_unit::tlb_flush_kernel(Address /*va*/)
{}

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
