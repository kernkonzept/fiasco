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

  struct Ptab_cfg
  {
    Address pt_phys_addr;
    unsigned char virt_addr_size;
    unsigned char start_level;
  };
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
IMPLEMENTATION [iommu && !arm_iommu_stage2]:

PRIVATE inline
Dmar_space::Ptab_cfg
Dmar_space::get_ptab_cfg(Iommu *)
{
  unsigned char virt_addr_size = Dmar_pdir::page_order_for_level(0)
                                 + Dmar_pdir::Levels::size(0);
  // The start level is irrelevant for stage 1 PTs...
  return {Mem_layout::pmem_to_phys(_dmarpt), virt_addr_size, 0};
}

// -----------------------------------------------------------
IMPLEMENTATION [iommu && arm_iommu_stage2]:

IMPLEMENT_OVERRIDE inline
bool
Dmar_space::prealloc_pt()
{
  // Force allocation of the first level 1 page table entry. Required to
  // support SMMUs that require to start at level 1 instead of level 0. See
  // get_ptab_cfg() below.
  auto i = _dmarpt->walk(Virt_addr(0), 1, Dmar_pte_ptr::need_cache_write_back(),
                         Kmem_alloc::q_allocator(ram_quota()));
  return i.level == 1;
}

PRIVATE inline
Dmar_space::Ptab_cfg
Dmar_space::get_ptab_cfg(Iommu *iommu)
{
  constexpr unsigned max_ipa_size = Dmar_pdir::page_order_for_level(0)
                                    + Dmar_pdir::Levels::size(0);
  unsigned char ias = min(iommu->ipa_size(), max_ipa_size);
  if (ias >= 44)
    // From 44 bits and above, use 4 levels and start at level 0
    return {Mem_layout::pmem_to_phys(_dmarpt), ias, 2};

  // There is an unusable range between 40 and 43. Such hardware requires to
  // start at level 1 (see ARM IHI 0070 STE.S2T0SZ description in conjunction
  // with ARM DDI 0487 section D5.2.3, "Controlling Address translation
  // stages"). Because we have no concatenated first level table, constrain the
  // input size. :-(
  if (ias >= 40)
    {
      static bool warned = false;
      if (!warned)
        {
          warned = true;
          WARN("IOMMU: hardware supports more bits (%d) than the kernel can use!"
               " DMA addresses constrained to 39 bits.\n", ias);
        }
      ias = 39;
    }

  // Skip the first level...
  auto pte = _dmarpt->walk(Virt_addr(0), 0);
  assert(pte.is_valid());
  return {pte.next_level(), ias, 1};
}

// -----------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "kmem.h"

IMPLEMENT
void
Dmar_space::init_page_sizes()
{
  add_page_size(Mem_space::Page_order(12));
  add_page_size(Mem_space::Page_order(21)); // 2 MiB
  add_page_size(Mem_space::Page_order(30)); // 1 GiB
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
  auto [pt_phys_addr, virt_addr_size, start_level] = get_ptab_cfg(mmu);
  return mmu->bind(stream_id, _domain, pt_phys_addr, virt_addr_size,
                   start_level);
}

IMPLEMENT
int
Dmar_space::unbind_mmu(Iommu *mmu, Unsigned32 stream_id)
{
  auto pt_phys_addr = get_ptab_cfg(mmu).pt_phys_addr;
  return mmu->unbind(stream_id, _domain, pt_phys_addr);
}

PRIVATE
void
Dmar_space::remove_from_all_iommus()
{
  for (auto &iommu : Iommu::iommus())
    {
      auto pt_phys_addr = get_ptab_cfg(&iommu).pt_phys_addr;
      iommu.remove(_domain, pt_phys_addr);
    }
}
