INTERFACE [iommu]:

#include "task.h"
#include "ptab_base.h"
#include "bitmap.h"

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

    unsigned char page_order() const;
    Unsigned64 page_addr() const;
    Attr attribs() const
    {
      typedef L4_fpage::Rights R;

      auto raw = access_once(&e->v);

      R r = R::UR();
      if (raw & 2) r |= R::W();

      return Attr(r, Page::Type::Normal());
    }

    bool add_attribs(Page::Attr attr)
    {
      typedef L4_fpage::Rights R;

      if (attr.rights & R::W())
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

    L4_fpage::Rights access_flags() const
    {
      return L4_fpage::Rights(0);
    }

    void del_rights(L4_fpage::Rights r)
    {
      if (r & L4_fpage::Rights::W())
        {
          auto p = access_once(&e->v);
          auto o = p & ~(Unsigned64)2;
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
      typedef L4_fpage::Rights R;

      assert(level <= Dmar_pt::Depth);
      Unsigned64 r = (level == Dmar_pt::Depth) ? 0 : (Unsigned64)(1<<7);
      r |= 1; // Read
      if (attr.rights & R::W()) r |= 2;

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
  enum { Max_nr_did = 0x10000 };
  virtual void *debug_dir() const { return (void *)_dmarpt; }
  static void create_identity_map();
  static Dmar_pt *identity_map;

  void tlb_flush(bool) override;

private:
  Dmar_pt *_dmarpt;
  unsigned long _did;

  static bool _initialized;

  typedef Bitmap<Max_nr_did> Did_map;

  static Did_map *_free_dids;
  static unsigned _max_did;
};

// -----------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "boot_alloc.h"
#include "intel_iommu.h"
#include "kmem_slab.h"
#include "warn.h"

JDB_DEFINE_TYPENAME(Dmar_space, "DMA");

Dmar_space::Dmar_pt *Dmar_space::identity_map;
bool Dmar_space::_initialized;
Dmar_space::Did_map *Dmar_space::_free_dids;
unsigned Dmar_space::_max_did;


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

PUBLIC inline
Mword
Dmar_space::get_root(int aw_level) const
{
  return get_root(_dmarpt, aw_level);
}

PRIVATE static
unsigned
Dmar_space::alloc_did()
{
  /* DID 0 may be reserved by the architecture, DID 1 is identity map. */
  for (unsigned did = 2; did < _max_did; ++did)
    if (_free_dids->atomic_get_and_set(did) == false)
      return did;

  // No DID left
  return ~0U;
}

PRIVATE static
void
Dmar_space::free_did(unsigned long did)
{
  if (_free_dids->atomic_get_and_clear(did) != true)
    panic("DMAR: Freeing free DID");
}

PUBLIC static
void
Dmar_space::init(unsigned max_did)
{
  add_page_size(Mem_space::Page_order(Config::PAGE_SHIFT));
  /* XXX CEH: Add additional page sizes based on CAP_REG[34:37] */

  _max_did = max_did;
  _free_dids = new Boot_object<Did_map>();
  _initialized = true;
}

PUBLIC inline
bool
Dmar_space::initialize()
{
  void *b;

  if (!_initialized)
    return false;

  b = Kmem_alloc::allocator()->q_alloc(ram_quota(), Config::PAGE_SHIFT);
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
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::PAGE_SHIFT, _dmarpt);
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
unsigned long
Dmar_space::get_did()
{
  // XXX: possibly need a loop here
  if (_did == 0)
    {
      unsigned ndid = alloc_did();
      if (EXPECT_FALSE(ndid == ~0U))
        return ~0UL;

      if (!mp_cas(&_did, (unsigned long)0, (unsigned long)ndid))
        free_did(ndid);
    }
  return _did;
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
             (max_phys + Config::PAGE_SIZE - 1) >> Config::PAGE_SHIFT);

  printf("IOMMU: identity map 0 - 0x%llx (%lldGB)\n", epfn << Config::PAGE_SHIFT,
         (epfn << Config::PAGE_SHIFT) >> 30);
  for (Unsigned64 pfn = 0; pfn <= epfn; ++pfn)
    {
      auto i = identity_map->walk(Mem_space::V_pfn(pfn),
                                  Dmar_pt::Depth, false,
                                  Kmem_alloc::q_allocator(Ram_quota::root));
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
Dmar_space::tlb_flush(bool)
{
  if (_did)
    for (auto &mmu: Intel::Io_mmu::iommus)
      mmu.flush_iotlb(_did);
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
                     Mem_space::Attr page_attribs) override
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
L4_fpage::Rights
Dmar_space::v_delete(Mem_space::Vaddr virt, Mem_space::Page_order order,
                     L4_fpage::Rights page_attribs) override
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto i = _dmarpt->walk(virt);

  if (EXPECT_FALSE(!i.is_valid()))
    return L4_fpage::Rights(0);

  if (EXPECT_FALSE(Mem_space::Page_order(i.page_order()) != order))
    return L4_fpage::Rights(0);

  L4_fpage::Rights ret = i.access_flags();

  if (!(page_attribs & L4_fpage::Rights::R()))
    i.del_rights(page_attribs);
  else
    i.clear();

  return ret;
}

PUBLIC
void
Dmar_space::v_set_access_flags(Mem_space::Vaddr, L4_fpage::Rights) override
{}

static Mem_space::Fit_size::Size_array __dmar_ps;

PUBLIC
Mem_space::Fit_size
Dmar_space::mem_space_fitting_sizes() const override
{ return Mem_space::Fit_size(__dmar_ps); }

PRIVATE static
void
Dmar_space::add_page_size(Mem_space::Page_order o)
{
  add_global_page_size(o);
  for (Mem_space::Page_order c = o; c < __dmar_ps.size(); ++c)
    __dmar_ps[c] = o;
}

PUBLIC
void *
Dmar_space::operator new (size_t size, void *p) throw()
{
  (void)size;
  assert (size == sizeof (Dmar_space));
  return p;
}

PUBLIC
void
Dmar_space::operator delete (void *ptr)
{
  Dmar_space *t = reinterpret_cast<Dmar_space *>(ptr);
  Kmem_slab_t<Dmar_space>::q_free(t->ram_quota(), ptr);
}

PUBLIC inline
Dmar_space::Dmar_space(Ram_quota *q)
: Dyn_castable_class(q, Caps::mem()),
  _dmarpt(0), _did(0)
{}

PRIVATE
void
Dmar_space::remove_from_all_iommus()
{
  unsigned long did = access_once(&_did);
  if (!did)
    return;

  // someone else changed the did
  if (!mp_cas(&_did, did, 0ul))
    return;

  for (auto &mmu: Intel::Io_mmu::iommus)
    {
      for (unsigned bus = 0; bus < 255; ++bus)
        for (unsigned df = 0; df < 255; ++df)
          {
            auto entryp = mmu.get_context_entry(bus, df, false);
            if (!entryp)
              break; // complete bus is empty

            Intel::Io_mmu::Cte entry = access_once(entryp.unsafe_ptr());
            // different space bound, skip
            if (entry.slptptr() != get_root(mmu.aw()))
              continue;

            // when the CAS fails someone else already unbound this slot,
            // so ignore that case
            mmu.cas_context_entry(entryp, bus, df, entry, Intel::Io_mmu::Cte());
          }

      mmu.flush_iotlb(did);
    }

  free_did(did);
}

PUBLIC
void
Dmar_space::destroy(Kobject ***rl) override
{
  Task::destroy(rl);
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
      Kmem_alloc::allocator()->q_free(ram_quota(), Config::PAGE_SHIFT, _dmarpt);
      _dmarpt = 0;
    }
}

namespace {
static inline void __attribute__((constructor)) FIASCO_INIT
register_factory()
{
  Kobject_iface::set_factory(L4_msg_tag::Label_dma_space,
                             &Task::generic_factory<Dmar_space>);
}
}
