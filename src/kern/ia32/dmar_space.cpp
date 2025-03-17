INTERFACE [iommu]:

#include "task.h"
#include "ptab_base.h"
#include "simple_id_alloc.h"

class Dmar_space :
  public cxx::Dyn_castable<Dmar_space, Task>
{
private:
  template<typename T>
  static void clean_dcache(T *p)
  { Mem_unit::clean_dcache(p, p + 1); }

  class Dmar_ptr
  {
  public:
    struct Dmar_ptr_val
    {
      Unsigned64 v;
      Dmar_ptr_val() = default;
      Dmar_ptr_val(Unsigned64 v) : v(v) {}
      CXX_BITFIELD_MEMBER(0, 1, present, v);
    };

    Dmar_ptr_val *e;

  public:
    typedef Mem_space::Attr Attr;

    unsigned char level;
    Dmar_ptr() = default;
    Dmar_ptr(Unsigned64 *e, unsigned char l)
    : e(reinterpret_cast<Dmar_ptr_val*>(e)), level(l) {}

    bool is_valid() const { return e->present(); }
    bool is_leaf() const
    { return (level == Dmar_pt::Depth) || (e->v & (1 << 7)); }
    Unsigned64 next_level() const
    { return cxx::mask_lsb(e->v, Config::PAGE_SHIFT); }

    void set(Unsigned64 v);
    void clear() { set(0); }

    unsigned page_level() const;
    unsigned char page_order() const;
    Unsigned64 page_addr() const;
    Attr attribs() const
    {
      auto raw = access_once(&e->v);

      Page::Rights r = Page::Rights::UR();
      if (raw & 2) r |= Page::Rights::W();

      return Attr::space_local(r);
    }

    bool add_attribs(Page::Attr attr)
    {
      if (attr.rights & Page::Rights::W())
        {
          auto p = access_once(&e->v);
          auto o = p;
          p |= 2;
          if (o != p)
            {
              write_now(&e->v, p);
              clean_dcache(e);
              return true;
            }
        }
      return false;
    }

    void set_next_level(Unsigned64 phys)
    { set(phys | 3); }

    void write_back_if(bool) const {}
    static void write_back(void *, void *) {}

    Page::Flags access_flags() const
    {
      return Page::Flags::None();
    }

    void del_rights(Page::Rights r)
    {
      if (r & Page::Rights::W())
        {
          auto p = access_once(&e->v);
          auto o = p & ~2ULL;
          if (o != p)
            {
              write_now(&e->v, p);
              clean_dcache(e);
            }
        }
    }

    /*
     * WARNING: The VT-d documentation says that the super page bit
     * WARNING: is ignored in page table entries for 4k pages. However,
     * WARNING: this is not true. The super page bit must be zero.
     */
    void create_page(Phys_mem_addr addr, Page::Attr attr)
    {
      assert(level <= Dmar_pt::Depth);
      Unsigned64 r = (level == Dmar_pt::Depth) ? 0ULL : (1ULL << 7);
      r |= 1; // Read
      if (attr.rights & Page::Rights::W()) r |= 2;

      set(cxx::int_value<Phys_mem_addr>(addr) | r);
    }
  };

  typedef Ptab::Tupel<Ptab::Traits<Unsigned64, 39, 9, true>,
                      Ptab::Traits<Unsigned64, 30, 9, true>,
                      Ptab::Traits<Unsigned64, 21, 9, true>,
                      Ptab::Traits<Unsigned64, 12, 9, true> >::List Dmar_traits;

  typedef Ptab::Shift<Dmar_traits, 12>::List Dmar_traits_vpn;
  typedef Ptab::Page_addr_wrap<Page_number, 12> Dmar_va_vpn;
  typedef Ptab::Base<Dmar_ptr, Dmar_traits_vpn, Dmar_va_vpn, Mem_layout> Dmar_pt;

public:
  virtual void *debug_dir() const { return _dmarpt; }
  static void create_identity_map();
  static Dmar_pt *identity_map;

  void tlb_flush_current_cpu() override;
  using Did = unsigned long;

  enum
  {
    // DID 0 may be reserved by the architecture, DID 1 is identity map.
    First_did   = 2,
    Max_nr_did  = 0x10000,
    Invalid_did = ~0UL,
  };

private:
  Dmar_pt *_dmarpt;
  Did _did;

  static bool _initialized;

  using Did_alloc = Simple_id_alloc<Did, First_did, Max_nr_did, Invalid_did>;
  static Static_object<Did_alloc> _did_alloc;
};

// -----------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "boot_alloc.h"
#include "intel_iommu.h"
#include "kmem_slab.h"
#include "warn.h"
#include "paging_bits.h"

JDB_DEFINE_TYPENAME(Dmar_space, "DMA");

Dmar_space::Dmar_pt *Dmar_space::identity_map;
bool Dmar_space::_initialized;
Static_object<Dmar_space::Did_alloc> Dmar_space::_did_alloc;
static Kmem_slab_t<Dmar_space> _dmar_allocator("Dmar_space");


PUBLIC
Page_number
Dmar_space::mem_space_map_max_address() const override
{
  return Page_number(1UL << (Intel::Io_mmu::hw_addr_width - Mem_space::Page_shift));
}

PUBLIC static inline
Mword
Dmar_space::get_root(Dmar_pt *pt, unsigned aw_level)
{
  aw_level += 2;
  if (aw_level == Dmar_pt::Depth + 1)
    return Mem_layout::pmem_to_phys(pt);

  assert(aw_level <= Dmar_pt::Depth);

  auto i = pt->walk(Mem_space::V_pfn(0), Dmar_pt::Depth - aw_level);
  assert(i.is_valid());
  return i.next_level();
}

/**
 * Get the root page table for the given address width.
 *
 * The root page table is either the entire _dmarpt page table (4-level page
 * table) or the page table under its first entry (3-level page table).
 *
 * \param aw_level  1 if the IOMMU uses 3-level page table.
 *                  2 if the IOMMU uses 4-level page table.
 *
 * \return  The root page table of this Dmar_space.
 *
 * \note `Dmar_space::initialize()` pre-allocates the _dmarpt page table and the
 *       page table under its first entry. Therefore, it is safe to invoke
 *       `Dmar_space::get_root()` even while someone else is modifying the
 *       Dmar_space's page table.
 */
PUBLIC inline
Mword
Dmar_space::get_root(int aw_level) const
{
  return get_root(_dmarpt, aw_level);
}

PUBLIC static
void
Dmar_space::init(unsigned max_did)
{
  add_page_size(Mem_space::Page_order(Config::PAGE_SHIFT));
  /* XXX CEH: Add additional page sizes based on CAP_REG[34:37] */

  _did_alloc.construct(max_did);
  _initialized = true;
}

PUBLIC inline
bool
Dmar_space::initialize()
{
  void *b;

  if (!_initialized)
    return false;

  b = Kmem_alloc::allocator()->q_alloc(ram_quota(), Config::page_order());
  if (EXPECT_FALSE(!b))
    return false;

  _dmarpt = static_cast<Dmar_pt *>(b);
  _dmarpt->clear(false);

  /*
   * Make sure that the very first entry in a page table is valid and
   * not a super page. This is necessary if the hardware supports
   * fewer levels than the current software implementation.
   *
   * Force allocation of two levels in entry 0, so get_root works
   */
  auto i = _dmarpt->walk(Mem_space::V_pfn(0), 2, false,
                         Kmem_alloc::q_allocator(ram_quota()));
  if (i.level != 2)
    {
      // Got a page-table entry with the wrong level. That happens in the
      // case of an out-of-memory situation. So free everything we already
      // allocated and fail.
      _dmarpt->destroy(Virt_addr(0UL), Virt_addr(~0UL), 0, Dmar_pt::Depth,
                       Kmem_alloc::q_allocator(ram_quota()));
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::page_order(), _dmarpt);
      _dmarpt = 0;
      return false;
    }

  return true;
}

IMPLEMENT inline
void
Dmar_space::Dmar_ptr::set(Unsigned64 v)
{
  write_consistent(e, Dmar_ptr_val(v));
  clean_dcache(e);
}

PUBLIC inline
Dmar_space::Did
Dmar_space::get_did()
{
  return _did_alloc->get_or_alloc_id(&_did);
}

IMPLEMENT
void
Dmar_space::create_identity_map()
{
  if (identity_map)
    return;

  WARN("At least one IOMMU does not support passthrough.\n");

  check (identity_map = Kmem_alloc::allocator()->alloc_array<Dmar_pt>(1));
  identity_map->clear(false);

  Unsigned64 max_phys = 0;
  for (auto const &m: Kip::k()->mem_descs_a())
    if (m.valid() && m.type() == Mem_desc::Conventional
        && !m.is_virtual() && m.end() > max_phys)
      max_phys = m.end();

  Unsigned64 epfn;
  epfn = min(1ULL << (Intel::Io_mmu::hw_addr_width - Config::PAGE_SHIFT),
             Pg::count(max_phys + Config::PAGE_SIZE - 1));

  printf("IOMMU: identity map 0 - 0x%llx (%llu GiB)\n", Pg::size(epfn),
         Pg::size(epfn) >> 30);
  for (Unsigned64 pfn = 0; pfn <= epfn; ++pfn)
    {
      auto i = identity_map->walk(Mem_space::V_pfn(pfn),
                                  Dmar_pt::Depth, false,
                                  Kmem_alloc::q_allocator(Ram_quota::root.unwrap()));
      if (i.page_order() != 12)
        panic("IOMMU: cannot allocate identity IO page table, OOM\n");

      i.set((pfn << Config::PAGE_SHIFT) | 3);
    }
}


PUBLIC inline
int
Dmar_space::resume_vcpu(Context *, Vcpu_state *, bool) override
{
  return -L4_err::EInval;
}

IMPLEMENT inline
unsigned
Dmar_space::Dmar_ptr::page_level() const
{ return level; }

IMPLEMENT inline
unsigned char
Dmar_space::Dmar_ptr::page_order() const
{ return Dmar_space::Dmar_pt::page_order_for_level(level); }

IMPLEMENT inline
Unsigned64
Dmar_space::Dmar_ptr::page_addr() const
{
  unsigned char o = page_order();
  return cxx::mask_lsb(e->v, o);
}

IMPLEMENT
void
Dmar_space::tlb_flush_current_cpu()
{
  Did did = _did_alloc->get_id(&_did);
  if (did != Invalid_did)
    Intel::Io_mmu::queue_and_wait_on_all_iommus(
      Intel::Io_mmu::Inv_desc::iotlb_did(did));
}

PUBLIC
bool
Dmar_space::v_lookup(Mem_space::Vaddr virt, Mem_space::Phys_addr *phys,
                     Mem_space::Page_order *order,
                     Mem_space::Attr *page_attribs) override
{
  auto i = _dmarpt->walk(virt);
  // XXX CEH: Check if this hack is still needed!
  if (order)
    *order = Mem_space::Page_order(i.page_order() > 30 ? 30 : i.page_order());

  if (!i.is_valid())
    return false;

  if (phys)
    *phys = Mem_space::Phys_addr(i.page_addr());

  if (page_attribs)
    *page_attribs = i.attribs();

  return true;
}

PUBLIC
Mem_space::Status
Dmar_space::v_insert(Mem_space::Phys_addr phys, Mem_space::Vaddr virt,
                     Mem_space::Page_order order,
                     Mem_space::Attr page_attribs, bool) override
{
  assert (cxx::is_zero(cxx::get_lsb(Mem_space::Phys_addr(phys), order)));
  assert (cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level < Dmar_pt::Depth; ++level)
    if (Mem_space::Page_order(Dmar_pt::page_order_for_level(level)) <= order)
      break;

  auto i = _dmarpt->walk(virt, level, false,
                         Kmem_alloc::q_allocator(ram_quota()));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Mem_space::Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
      && (i.level != level || Mem_space::Phys_addr(i.page_addr()) != phys)))
    return Mem_space::Insert_err_exists;

  if (i.is_valid())
    {
      if (EXPECT_FALSE(!i.add_attribs(page_attribs)))
        return Mem_space::Insert_warn_exists;

      return Mem_space::Insert_warn_attrib_upgrade;
    }
  else
    {
      i.create_page(phys, page_attribs);
      return Mem_space::Insert_ok;
    }
}

PUBLIC
Page::Flags
Dmar_space::v_delete(Mem_space::Vaddr virt, Mem_space::Page_order order,
                     Page::Rights rights) override
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto pte = _dmarpt->walk(virt);

  if (EXPECT_FALSE(!pte.is_valid()))
    return Page::Flags::None();

  if (EXPECT_FALSE(Mem_space::Page_order(pte.page_order()) != order))
    return Page::Flags::None();

  Page::Flags flags = pte.access_flags();

  if (!(rights & Page::Rights::R()))
    pte.del_rights(rights);
  else
    pte.clear();

  return flags;
}

PUBLIC
void
Dmar_space::v_add_access_flags(Mem_space::Vaddr, Page::Flags) override
{}

static Mem_space::Fit_size __dmar_ps;

PUBLIC
Mem_space::Fit_size const &
Dmar_space::mem_space_fitting_sizes() const override
{ return __dmar_ps; }

PRIVATE static
void
Dmar_space::add_page_size(Mem_space::Page_order o)
{
  add_global_page_size(o);
  __dmar_ps.add_page_size(o);
}

PUBLIC static
Dmar_space *Dmar_space::alloc(Ram_quota *q)
{
  return _dmar_allocator.q_new(q, q);
}

PUBLIC
void *
Dmar_space::operator new ([[maybe_unused]] size_t size, void *p) noexcept
{
  assert (size == sizeof (Dmar_space));
  return p;
}

PUBLIC
void
Dmar_space::operator delete (Dmar_space *space, std::destroying_delete_t)
{
  Ram_quota *q = space->ram_quota();
  space->~Dmar_space();
  _dmar_allocator.q_free(q, space);
}

PUBLIC inline
Dmar_space::Dmar_space(Ram_quota *q)
: Dyn_castable_class(q, Caps::mem()),
  _dmarpt(0), _did(Invalid_did)
{
  _tlb_type = Tlb_iommu;
}

PRIVATE
void
Dmar_space::remove_from_all_iommus()
{
  if (!_initialized)
    return;

  Did did = _did_alloc->reset_id_if_valid(&_did);
  if (did == Invalid_did)
    return;

  bool need_wait[Intel::Io_mmu::Max_iommus];
  for (auto &mmu: Intel::Io_mmu::iommus)
    {
      need_wait[mmu.idx()] = false;

      for (unsigned bus = 0; bus < 255; ++bus)
        for (unsigned df = 0; df < 255; ++df)
          {
            auto entryp = mmu.get_context_entry(bus, df, false);
            if (!entryp)
              break; // complete bus is empty

            // There is no need for grabbing the IOMMU lock when accessing the
            // entry, since remove_from_all_iommus() is only used during the
            // destruction of a Dmar_space:
            // 1. From destroy(), which is invoked during the first destruction
            //    phase, i.e. before waiting for RCU grace period. We might miss
            //    some entries here, when entries are created for Dmar_space
            //    concurrently. In addition these entries might have a different
            //    DID, because remove_from_all_iommus() resets the DID.
            //
            // 2. From ~Dmar_space(), which is invoked during the second
            //    destruction phase, i.e. after waiting the RCU grace period.
            //    Now we clean up all the remaining context entries, if any were
            //    created since the first invocation of remove_from_all_iommus().
            Intel::Io_mmu::Cte entry = access_once(entryp.unsafe_ptr());
            // different space bound, skip
            if (entry.slptptr() != get_root(mmu.aw()))
              continue;

            // when the CAS fails someone else already unbound this slot,
            // so ignore that case
            mmu.cas_context_entry(entryp, bus, df, entry, Intel::Io_mmu::Cte(),
                                  &need_wait[mmu.idx()]);
          }
    }

  _did_alloc->free_id(did);
  Intel::Io_mmu::queue_and_wait_on_iommus(need_wait);
}

PUBLIC
void
Dmar_space::destroy(Kobjects_list &reap_list) override
{
  Task::destroy(reap_list);
  remove_from_all_iommus();
}

PUBLIC
Dmar_space::~Dmar_space()
{
  remove_from_all_iommus();

  if (_dmarpt)
    {
      _dmarpt->destroy(Virt_addr(0UL), Virt_addr(~0UL), 0, Dmar_pt::Depth,
                       Kmem_alloc::q_allocator(ram_quota()));
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::page_order(), _dmarpt);
      _dmarpt = 0;
    }
}

namespace {

static inline
void __attribute__((constructor)) FIASCO_INIT_SFX(dmar_space_register_factory)
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_dma_space,
                             &Task::generic_factory<Dmar_space>);
}

}
