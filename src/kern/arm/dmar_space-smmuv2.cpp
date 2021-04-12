// -----------------------------------------------------------
INTERFACE [iommu && (iommu_arm_smmu_400 || 32bit)]:

EXTENSION class Dmar_space
{
  // LPAE page table format
  typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 2, true>,
                       Ptab::Traits< Unsigned64, 21, 9, true>,
                       Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;
};

// -----------------------------------------------------------
INTERFACE [iommu && iommu_arm_smmu_500 && 64bit]:

EXTENSION class Dmar_space
{
  // AArch64 page table format, with a concatenated root page table (10-bits),
  // which means we skip level zero.
  typedef Ptab::Tupel< Ptab::Traits< Unsigned64, 30, 10, true>,
                       Ptab::Traits< Unsigned64, 21, 9, true>,
                       Ptab::Traits< Unsigned64, 12, 9, true> >::List Ptab_traits;
};

// -----------------------------------------------------------
INTERFACE [iommu]:

EXTENSION class Dmar_space
{
  class Pte_ptr :
    public Pte_long_desc<Pte_ptr>,
    public Pte_iommu<Pte_ptr>,
    public Pte_stage2_attribs<Pte_ptr, Page>,
    public Pte_generic<Pte_ptr, Unsigned64>
  {
  public:
    enum
    {
      Super_level = 1,
      Max_level   = 2,
    };
    Pte_ptr() = default;
    Pte_ptr(void *p, unsigned char level) : Pte_long_desc<Pte_ptr>(p, level) {}

    unsigned char page_order() const
    {
      return Ptab::Level<Dmar_ptab_traits_vpn>::shift(level)
        + Dmar_ptab_traits_vpn::Head::Base_shift;
    }
  };

  using Dmar_ptab_traits_vpn = Ptab::Shift<Ptab_traits, Virt_addr::Shift>::List;

  using Dmar_pdir = Pdir_t<Pte_ptr, Dmar_ptab_traits_vpn, Ptab_va_vpn>;
  Dmar_pdir *_dmarpt;

  using Dmarpt_alloc = Kmem_slab_t<Dmar_pdir, sizeof(Dmar_pdir)>;
  static Dmarpt_alloc _dmarpt_alloc;

  Iommu::Space_id _space_id;
};

// -----------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "kmem.h"

IMPLEMENT
void
Dmar_space::init_page_sizes()
{
  add_page_size(Mem_space::Page_order(12));
  add_page_size(Mem_space::Page_order(21));
  add_page_size(Mem_space::Page_order(30));
}

IMPLEMENT
void
Dmar_space::tlb_flush(bool)
{
  Iommu::tlb_invalidate_space(_space_id);
}

IMPLEMENT
int
Dmar_space::bind_mmu(Iommu *mmu, Unsigned32 stream_id)
{
  return mmu->bind(stream_id, pt_phys_addr(), &_space_id);
}

IMPLEMENT
int
Dmar_space::unbind_mmu(Iommu *mmu, Unsigned32 stream_id)
{
  return mmu->unbind(stream_id, pt_phys_addr());
}

PRIVATE
void
Dmar_space::remove_from_all_iommus()
{
  for (auto &iommu : Iommu::iommus())
    iommu.remove(pt_phys_addr());
}
