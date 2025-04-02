INTERFACE [iommu]:

#include "task.h"
#include "ptab_base.h"
#include "paging.h"
#include "iommu.h"

class Dmar_space :
  public cxx::Dyn_castable<Dmar_space, Task>
{
public:
  void tlb_flush_current_cpu() override;
  int bind_mmu(Iommu *mmu, Unsigned32 stream_id);
  int unbind_mmu(Iommu *mmu, Unsigned32 stream_id);

private:
  /**
   * Mixin for PTE pointers for IOMMUs.
   */
  template<typename CLASS>
  struct Pte_iommu
  {
    static bool need_cache_write_back()
    { return !Iommu::Coherent; }

    void write_back_if(bool)
    { write_back(); }

    void write_back()
    {
      if (!Iommu::Coherent)
        Mem_unit::clean_dcache(static_cast<CLASS const *>(this)->pte);
    }

    static void write_back(void *start, void *end)
    {
      if (!Iommu::Coherent)
        Mem_unit::clean_dcache(start, end);
    }
  };

  static void init_page_sizes();

  static bool _initialized;
};

// -----------------------------------------------------------
IMPLEMENTATION [iommu]:

#include "boot_alloc.h"
#include "iommu.h"
#include "kmem_slab.h"
#include "warn.h"

// TODO: Inspection of DMAR page tables with JDB fails, because JDB uses the
//       wrong page table layout (the one of regular address spaces).
JDB_DEFINE_TYPENAME(Dmar_space, "DMA");

bool Dmar_space::_initialized;

PUBLIC static
void
Dmar_space::init()
{
  Dmar_space::init_page_sizes();
  _initialized = true;
}

Dmar_space::Dmarpt_alloc Dmar_space::_dmarpt_alloc;

PUBLIC inline
bool
Dmar_space::initialize()
{
  if (!_initialized)
    return false;

  _dmarpt = _dmarpt_alloc.q_new(ram_quota());
  if (!_dmarpt)
    return false;

  _dmarpt->clear(Pte_ptr::need_cache_write_back());

  return true;
}

PUBLIC inline
int
Dmar_space::resume_vcpu(Context *, Vcpu_state *, bool) override
{
  return -L4_err::EInval;
}

PUBLIC
bool
Dmar_space::v_lookup(Mem_space::Vaddr virt, Mem_space::Phys_addr *phys,
                     Mem_space::Page_order *order,
                     Mem_space::Attr *page_attribs) override
{
  auto i = _dmarpt->walk(virt);
  if (order) *order = Mem_space::Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  if (phys) *phys = Mem_space::Phys_addr(i.page_addr());
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

PUBLIC
Mem_space::Status
Dmar_space::v_insert(Mem_space::Phys_addr phys, Mem_space::Vaddr virt,
                     Mem_space::Page_order order,
                     Mem_space::Attr page_attribs, bool) override
{
  assert(cxx::is_zero(cxx::get_lsb(phys, order)));
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level <= Dmar_pdir::Depth; ++level)
    if (Mem_space::Page_order(Dmar_pdir::page_order_for_level(level)) <= order)
      break;

  auto i = _dmarpt->walk(virt, level, Pte_ptr::need_cache_write_back(),
                         Kmem_alloc::q_allocator(ram_quota()));

  if (EXPECT_FALSE(!i.is_valid() && i.level != level))
    return Mem_space::Insert_err_nomem;

  if (EXPECT_FALSE(i.is_valid()
      && (i.level != level || Mem_space::Phys_addr(i.page_addr()) != phys)))
    return Mem_space::Insert_err_exists;

  bool const valid = i.is_valid();
  if (valid)
    page_attribs.rights |= i.attribs().rights;

  auto entry = i.make_page(phys, page_attribs);

  if (valid)
    {
      if (EXPECT_FALSE(i.entry() == entry))
        return Mem_space::Insert_warn_exists;

      i.set_page(entry);
      i.write_back();
      return Mem_space::Insert_warn_attrib_upgrade;
    }
  else
    {
      i.set_page(entry);
      i.write_back();
      return Mem_space::Insert_ok;
    }
}

PUBLIC
Page::Flags
Dmar_space::v_delete(Mem_space::Vaddr virt,
                     [[maybe_unused]] Mem_space::Page_order order,
                     Page::Rights rights) override
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto pte = _dmarpt->walk(virt);

  if (EXPECT_FALSE(!pte.is_valid()))
    return Page::Flags::None();

  Page::Flags flags = pte.access_flags();

  if (!(rights & Page::Rights::R()))
    pte.del_rights(rights);
  else
    pte.clear();

  pte.write_back();

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

static Kmem_slab_t<Dmar_space> _dmar_space_allocator("Dmar_space");

PUBLIC static
Dmar_space *Dmar_space::alloc(Ram_quota *q)
{
  return _dmar_space_allocator.q_new(q, q);
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
  _dmar_space_allocator.q_free(q, space);
}

PUBLIC inline
Dmar_space::Dmar_space(Ram_quota *q)
: Dyn_castable_class(q, Caps::mem()),
  _dmarpt(nullptr)
{
  _tlb_type = Tlb_iommu;
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
      _dmarpt->destroy(Virt_addr(0UL), Virt_addr(~0UL), 0, Dmar_pdir::Depth,
                       Kmem_alloc::q_allocator(ram_quota()));
      _dmarpt_alloc.q_free(ram_quota(), _dmarpt);
      _dmarpt = nullptr;
    }
}

PUBLIC inline
Address
Dmar_space::pt_phys_addr() const
{
  return Mem_layout::pmem_to_phys(_dmarpt);
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
