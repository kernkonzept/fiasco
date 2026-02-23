// -----------------------------------------------------------
INTERFACE [iommu && 64bit]:

EXTENSION class Dmar_space
{
  typedef Ptab::Tupel<Ptab::Traits<Unsigned64, 39, 9, false>,
                      Ptab::Traits<Unsigned64, 30, 9, true>,
                      Ptab::Traits<Unsigned64, 21, 9, true>,
                      Ptab::Traits<Unsigned64, 12, 9, true> >::List Dmar_traits;
  typedef Ptab::Shift<Dmar_traits, 12>::List Dmar_traits_vpn;
  typedef Ptab::Page_addr_wrap<Page_number, 12> Dmar_va_vpn;

  // For 4-level stage 2 page tables, we start at level zero. Not defined or
  // used for stage 1 page tables.
  static constexpr unsigned start_level()
  { return 2; }

};

// -----------------------------------------------------------
INTERFACE [iommu && !arm_iommu_stage2 && 64bit]:

EXTENSION class Dmar_space
{
  struct Stage1_page_attr
  {
    // See Iommu::Mair0_bits for the definition.
    enum Attribs_enum
    {
      Cache_mask    = 0x01c, ///< MAIR index 0..7
      NONCACHEABLE  = 0x000, ///< MAIR Attr0: Caching is off
      CACHEABLE     = 0x008, ///< MAIR Attr2: Cache is enabled
      BUFFERED      = 0x004, ///< MAIR Attr1: Write buffer enabled -- Normal, non-cached
    };

    enum
    {
      /// The NS-EL1&0 translation regime supports two privilege levels.
      Priv_levels = 2,
      PXN = 1ULL << 53, ///< Privileged Execute Never
      UXN = 1ULL << 54, ///< Unprivileged Execute Never
      XN = 0,           ///< Execute Never feature not available
    };
  };

  class Dmar_pte_ptr :
    public Pte_long_desc<Dmar_pte_ptr>,
    public Pte_iommu<Dmar_pte_ptr>,
    public Pte_long_attribs<Dmar_pte_ptr, Stage1_page_attr>,
    public Pte_generic<Dmar_pte_ptr, Unsigned64>
  {
  public:
    static constexpr unsigned super_level() { return 2; }
    static constexpr unsigned max_level()   { return 3; }
    Dmar_pte_ptr() = default;
    Dmar_pte_ptr(void *p, unsigned char level) : Pte_long_desc<Dmar_pte_ptr>(p, level) {}

    unsigned char page_order() const
    {
      return Ptab::Level<Dmar_traits_vpn>::shift(level)
        + Dmar_traits_vpn::Head::Base_shift;
    }
  };
};

// -----------------------------------------------------------
INTERFACE [iommu && arm_iommu_stage2 && cpu_virt && 64bit]:

EXTENSION class Dmar_space
{
  struct Stage2_page_attr
  {
    enum Attribs_enum : Mword
    {
      Cache_mask    = 0x03c,
      NONCACHEABLE  = 0x000, ///< Caching is off
      CACHEABLE     = 0x03c, ///< Cache is enabled
      BUFFERED      = 0x014, ///< Write buffer enabled -- Normal, non-cached
    };
  };

  class Dmar_pte_ptr :
    public Pte_long_desc<Dmar_pte_ptr>,
    public Pte_iommu<Dmar_pte_ptr>,
    public Pte_stage2_attribs<Dmar_pte_ptr, Stage2_page_attr>,
    public Pte_generic<Dmar_pte_ptr, Unsigned64>
  {
  public:
    static constexpr unsigned super_level() { return 2; }
    static constexpr unsigned max_level()   { return 3; }
    Dmar_pte_ptr() = default;
    Dmar_pte_ptr(void *p, unsigned char level) : Pte_long_desc<Dmar_pte_ptr>(p, level) {}

    unsigned char page_order() const
    {
      return Ptab::Level<Dmar_traits_vpn>::shift(level)
        + Dmar_traits_vpn::Head::Base_shift;
    }
  };
};

// -----------------------------------------------------------
INTERFACE [iommu]:

EXTENSION class Dmar_space
{
  using Dmar_pdir = Pdir_t<Dmar_pte_ptr, Dmar_traits_vpn, Dmar_va_vpn>;
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
  add_page_size(Mem_space::Page_order(21)); // 2 MiB
  add_page_size(Mem_space::Page_order(30)); // 1 GiB
}

PRIVATE inline
Address
Dmar_space::pt_phys_addr() const
{
  return Mem_layout::pmem_to_phys(_dmarpt);
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
