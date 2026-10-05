INTERFACE [iommu]:

#include "task.h"
#include "ptab_base.h"
#include "paging.h"
#include "iommu.h"

class Dmar_space :
  public cxx::Dyn_castable<Dmar_space, Task>
{
public:
  virtual int bind_mmu(Iommu *mmu, Unsigned32 stream_id, Unsigned64 *max_addr) = 0;
  virtual int unbind_mmu(Iommu *mmu, Unsigned32 stream_id) = 0;

  static bool _initialized;
};

template<typename DMAR_IMPL>
class Dmar_space_t : public Dmar_space
{
public:
  using Dmar_space::Dmar_space;

protected:
  DMAR_IMPL _impl;
};

/**
 * Mixin for PTE pointers for IOMMUs.
 */
template<typename CLASS>
struct Dmar_pte_iommu
{
  static bool need_cache_write_back()
  { return !Iommu::coherent(); }

  void write_back_if(bool)
  { write_back(); }

  void write_back()
  {
    if (need_cache_write_back())
      Mem_unit::clean_dcache(static_cast<CLASS const *>(this)->pte);
  }

  static void write_back(void *start, void *end)
  {
    if (need_cache_write_back())
      Mem_unit::clean_dcache(start, end);
  }
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

PROTECTED static
void
Dmar_space::init()
{
  _initialized = true;
}

PUBLIC inline
Dmar_space::Dmar_space(Ram_quota *q)
: Dyn_castable_class(q, Caps::mem())
{
  _tlb_type = Tlb_iommu;
}

PUBLIC inline
int
Dmar_space::resume_vcpu(Context *, Vcpu_state *, bool) override
{
  return -L4_err::EInval;
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

PROTECTED static
void
Dmar_space::add_page_size(Mem_space::Page_order o)
{
  add_global_page_size(o);
  __dmar_ps.add_page_size(o);
}

PUBLIC static template<typename DMAR_IMPL>
void
Dmar_space_t<DMAR_IMPL>::init()
{
  Dmar_space::init();
  DMAR_IMPL::init_page_sizes(&Dmar_space::add_page_size);
}

PUBLIC template<typename DMAR_IMPL> inline
bool
Dmar_space_t<DMAR_IMPL>::initialize()
{
  if (!_initialized)
    return false;

  return _impl.alloc_pt(ram_quota());
}

PUBLIC template<typename DMAR_IMPL>
bool
Dmar_space_t<DMAR_IMPL>::v_lookup(Mem_space::Vaddr virt,
                                  Mem_space::Phys_addr *phys,
                                  Mem_space::Page_order *order,
                                  Mem_space::Attr *page_attribs) override
{
  auto i = _impl.pt()->walk(virt);
  if (order) *order = Mem_space::Page_order(i.page_order());

  if (!i.is_valid())
    return false;

  if (phys) *phys = Mem_space::Phys_addr(i.page_addr());
  if (page_attribs) *page_attribs = i.attribs();

  return true;
}

PUBLIC template<typename DMAR_IMPL>
Mem_space::Status
Dmar_space_t<DMAR_IMPL>::v_insert(Mem_space::Phys_addr phys,
                                  Mem_space::Vaddr virt,
                                  Mem_space::Page_order order,
                                  Mem_space::Attr page_attribs, bool) override
{
  assert(cxx::is_zero(cxx::get_lsb(phys, order)));
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  int level;
  for (level = 0; level <= DMAR_IMPL::Dmar_pdir::Depth; ++level)
    if (Mem_space::Page_order(DMAR_IMPL::Dmar_pdir::page_order_for_level(level)) <= order)
      break;

  auto i = _impl.pt()->walk(virt, level, DMAR_IMPL::Dmar_pte_ptr::need_cache_write_back(),
                         Kmem_alloc::q_allocator(ram_quota()));

  if (!i.is_valid() && i.level != level) [[unlikely]]
    return Mem_space::Insert_err_nomem;

  if (i.is_valid()
      && (i.level != level || Mem_space::Phys_addr(i.page_addr()) != phys)) [[unlikely]]
    return Mem_space::Insert_err_exists;

  bool const valid = i.is_valid();
  if (valid)
    page_attribs.rights |= i.attribs().rights;

  auto entry = i.make_page(phys, page_attribs);

  if (valid)
    {
      if (i.entry() == entry) [[unlikely]]
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

PUBLIC template<typename DMAR_IMPL>
Page::Flags
Dmar_space_t<DMAR_IMPL>::v_delete(Mem_space::Vaddr virt,
                                  [[maybe_unused]] Mem_space::Page_order order,
                                  Page::Rights rights) override
{
  assert(cxx::is_zero(cxx::get_lsb(Virt_addr(virt), order)));

  auto pte = _impl.pt()->walk(virt);

  if (!pte.is_valid()) [[unlikely]]
    return Page::Flags::None();

  Page::Flags flags = pte.access_flags();

  if (!(rights & Page::Rights::R()))
    pte.del_rights(rights);
  else
    pte.clear();

  pte.write_back();

  return flags;
}

PUBLIC template<typename DMAR_IMPL>
void
Dmar_space_t<DMAR_IMPL>::destroy(Kobjects_list &reap_list) override
{
  Task::destroy(reap_list);
  _impl.remove_from_all_iommus();
}

PUBLIC static template<typename DMAR_IMPL>
Dmar_space_t<DMAR_IMPL> *
Dmar_space_t<DMAR_IMPL>::alloc(Ram_quota *q)
{
  return DMAR_IMPL::alloc_space(q);
}

PUBLIC template<typename DMAR_IMPL>
void *
Dmar_space_t<DMAR_IMPL>::operator new ([[maybe_unused]] size_t size, void *p) noexcept
{
  assert (size == sizeof (Dmar_space_t<DMAR_IMPL>));
  return p;
}

PUBLIC template<typename DMAR_IMPL>
void
Dmar_space_t<DMAR_IMPL>::operator delete (Dmar_space_t<DMAR_IMPL> *space, std::destroying_delete_t)
{
  Ram_quota *q = space->ram_quota();
  DMAR_IMPL::free_space(q, space);
}

PUBLIC template<typename DMAR_IMPL>
Dmar_space_t<DMAR_IMPL>::~Dmar_space_t() override
{
  _impl.remove_from_all_iommus();
  _impl.free_pt(ram_quota());
}

PUBLIC template<typename DMAR_IMPL>
void
Dmar_space_t<DMAR_IMPL>::tlb_flush_current_cpu() override
{
  _impl.tlb_flush_current_cpu();
}

PUBLIC template<typename DMAR_IMPL>
int
Dmar_space_t<DMAR_IMPL>::bind_mmu(Iommu *mmu, Unsigned32 stream_id,
                                  Unsigned64 *max_addr) override
{
  return _impl.bind_mmu(mmu, stream_id, max_addr);
}

PUBLIC template<typename DMAR_IMPL>
int
Dmar_space_t<DMAR_IMPL>::unbind_mmu(Iommu *mmu, Unsigned32 stream_id) override
{
  return _impl.unbind_mmu(mmu, stream_id);
}
