// -----------------------------------------------------------
INTERFACE [iommu && !arm_iommu_stage2 && 64bit]:

/**
 * When the SMMU is configured to use stage 1 translation, we use the same
 * page table layout for the SMMU as for the kernel (with a small difference,
 * see Iommu_page_attr). For a non-virtualization-enabled kernel, this is
 * equivalent to the regular memory space page table layout. However, for a
 * virtualization-enabled kernel, memory spaces use a stage 2 page table layout,
 * whereas the kernel uses a stage 1 page table layout.
 */
EXTENSION class Dmar_space
{
  static constexpr unsigned start_level()
  {
    // Not defined or used for stage 1 page tables.
    return 0;
  }

  /// For the SMMU we always use the NS-EL1&0 translation regime, just like the
  /// kernel does with virtualization disabled, whereas with virtualization
  /// enabled the kernel uses the EL2 translation regime. Therefore, we override
  /// the corresponding attributes here.
  struct Iommu_page_attr : public Kernel_page_attr
  {
    enum
    {
      /// The NS-EL1&0 translation regime supports two privilege levels.
      Priv_levels = 2,
      PXN = 1ULL << 53, ///< Privileged Execute Never
      UXN = 1ULL << 54, ///< Unprivileged Execute Never
      XN = 0,           ///< Execute Never feature not available
    };
  };

  class Pte_ptr :
    public Pte_long_desc<Pte_ptr>,
    public Pte_iommu<Pte_ptr>,
    public Pte_long_attribs<Pte_ptr, Iommu_page_attr>,
    public Pte_generic<Pte_ptr, Unsigned64>
  {
  public:
    enum
    {
      Super_level = ::K_pte_ptr::Super_level,
      Max_level   = ::K_pte_ptr::Max_level,
    };
    Pte_ptr() = default;
    Pte_ptr(void *p, unsigned char level) : Pte_long_desc<Pte_ptr>(p, level) {}

    unsigned char page_order() const
    {
      return Ptab::Level<Dmar_ptab_traits_vpn>::shift(level)
        + Dmar_ptab_traits_vpn::Head::Base_shift;
    }
  };

  using Dmar_ptab_traits_vpn = K_ptab_traits_vpn;
};

// -----------------------------------------------------------
INTERFACE [iommu && arm_iommu_stage2 && cpu_virt && 64bit]:

/**
 * When the SMMU is configured to use stage 2 translation, we use the regular
 * stage 2 page table layout also for the SMMU.
 */
EXTENSION class Dmar_space
{
public:
  static constexpr unsigned start_level()
  {
    // For non-ARM_PT48 the first page table is concatenated (10-bits), we skip
    // level zero.
    return Page::Vtcr_sl0;
  }

private:
  class Pte_ptr :
    public Pte_long_desc<Pte_ptr>,
    public Pte_iommu<Pte_ptr>,
    public Pte_stage2_attribs<Pte_ptr, Page>,
    public Pte_generic<Pte_ptr, Unsigned64>
  {
  public:
    enum
    {
      Super_level = ::Pte_ptr::Super_level,
      Max_level   = ::Pte_ptr::Max_level,
    };
    Pte_ptr() = default;
    Pte_ptr(void *p, unsigned char level) : Pte_long_desc<Pte_ptr>(p, level) {}

    unsigned char page_order() const
    {
      return Ptab::Level<Dmar_ptab_traits_vpn>::shift(level)
        + Dmar_ptab_traits_vpn::Head::Base_shift;
    }
  };

  using Dmar_ptab_traits_vpn = Ptab_traits_vpn;
};

// -----------------------------------------------------------
INTERFACE [iommu]:

EXTENSION class Dmar_space
{
  using Dmar_pdir = Pdir_t<Pte_ptr, Dmar_ptab_traits_vpn, Ptab_va_vpn>;
  Dmar_pdir *_dmarpt;

  using Dmarpt_alloc = Kmem_slab_t<Dmar_pdir, sizeof(Dmar_pdir)>;
  static Dmarpt_alloc _dmarpt_alloc;

  Iommu_domain _domain;
};

// -----------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "kmem.h"

PUBLIC static inline
unsigned
Dmar_space::virt_addr_size()
{
  return Dmar_pdir::page_order_for_level(0) + Dmar_pdir::Levels::size(0);
}

IMPLEMENT
void
Dmar_space::init_page_sizes()
{
  add_page_size(Mem_space::Page_order(12));
  add_page_size(Mem_space::Page_order(21)); // 2MB
  add_page_size(Mem_space::Page_order(30)); // 1GB
}

IMPLEMENT
void
Dmar_space::tlb_flush_current_cpu()
{
  Iommu::tlb_invalidate_domain(_domain);
}

IMPLEMENT
int
Dmar_space::bind_mmu(Iommu *mmu, Unsigned32 stream_id)
{
  return mmu->bind(stream_id, _domain, pt_phys_addr(),
                   virt_addr_size(), Dmar_space::start_level());
}

IMPLEMENT
int
Dmar_space::unbind_mmu(Iommu *mmu, Unsigned32 stream_id)
{
  return mmu->unbind(stream_id, _domain, pt_phys_addr());
}

PRIVATE
void
Dmar_space::remove_from_all_iommus()
{
  for (auto &iommu : Iommu::iommus())
    iommu.remove(_domain, pt_phys_addr());
}
