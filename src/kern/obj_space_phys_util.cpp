// ------------------------------------------------------------------------
INTERFACE [!obj_space_phys_avl]:

#include "assert_opt.h"
#include "obj_space_types.h"
#include "ram_quota.h"
#include "paging.h"

// Two level page-sized mapping table
template< typename SPACE >
class Obj_space_phys
{
public:
  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;

  Obj_space_phys() : _dir(nullptr)
  {}

  bool initialize()
  { return alloc_dir(); }

  bool v_lookup(V_pfn const &virt, Phys_addr *phys,
                Page_order *order, Attr *attribs);

  Page::Flags v_delete(V_pfn virt, Page_order order, Page::Rights rights);
  Obj::Insert_result v_insert(Phys_addr phys, V_pfn const &virt,
                              Page_order order, Attr page_attribs);

  Capability lookup(Cap_index virt);

private:
  enum
  {
    Slots_per_dir = Config::PAGE_SIZE / sizeof(void*),
    Map_max_address = Slots_per_dir * Obj::Caps_per_page,
  };

  struct Cap_table { Entry e[Obj::Caps_per_page]; };
  struct Cap_dir   { Cap_table *d[Slots_per_dir]; };
  Cap_dir *_dir;

  Ram_quota *ram_quota() const
  {
    assert_opt (this);
    return SPACE::ram_quota(this);
  }
};

// ------------------------------------------------------------------------
INTERFACE [obj_space_phys_avl]:

#include "assert_opt.h"
#include "obj_space_types.h"
#include "ram_quota.h"
#include "cxx/avl_map"
#include "kmem_slab.h"
#include "spin_lock.h"
#include "global_data.h"
#include "paging.h"

template< typename _Type >
class Obj_space_phys_allocator
{
public:
  typedef _Type Type;
  typedef Kmem_slab_t<Type> Allocator;

private:
  static Global_data<Allocator> _cap_allocator;

  Ram_quota *_q;

public:
  enum { can_free = true };

  Obj_space_phys_allocator(Ram_quota *q) noexcept : _q(q) {}
  Obj_space_phys_allocator(Obj_space_phys_allocator const &other) noexcept
  : _q(other._q)
  {}

  _Type *alloc() noexcept
  { return static_cast<_Type*>(_cap_allocator->q_alloc(_q)); }

  void free(_Type *t) noexcept
  { _cap_allocator->q_free(_q, t); }
};

typedef cxx::Avl_map<Cap_index, Obj::Entry, cxx::Lt_functor,
                     Obj_space_phys_allocator> Obj_space_phys_entries;

// AVL tree based mapping table
template< typename SPACE >
class Obj_space_phys
{
public:
  typedef Obj::Attr Attr;
  typedef Obj::Capability Capability;
  typedef Obj::Entry Entry;
  typedef Kobject_iface *Phys_addr;

  typedef Obj::Cap_addr V_pfn;
  typedef Cap_diff V_pfc;
  typedef Order Page_order;

  bool initialize()
  { return true; }

  bool v_lookup(V_pfn const &virt, Phys_addr *phys,
                Page_order *order, Attr *attribs);

  Page::Flags v_delete(V_pfn virt, Page_order order, Page::Rights rights);
  Obj::Insert_result v_insert(Phys_addr phys, V_pfn const &virt,
                              Page_order order, Attr page_attribs);

  Capability lookup(Cap_index virt);

private:
  enum
  {
    Map_max_address = 1UL << 20, /* 20bit obj index */
  };

  using Entries = Obj_space_phys_entries;
  Entries _map;
  Spin_lock<> _lock;

  Ram_quota *ram_quota() const
  {
    assert_opt (this);
    return SPACE::ram_quota(this);
  }
};

// ------------------------------------------------------------------------
IMPLEMENTATION [obj_space_phys_avl]:

template<>
DEFINE_GLOBAL
Global_data<Obj_space_phys_entries::Node_allocator::Allocator>
Obj_space_phys_allocator<Obj_space_phys_entries::Node_allocator::Type>::_cap_allocator("Cap");

// ------------------------------------------------------------------------
INTERFACE [obj_space_virt]:

#include "cxx/type_traits"

/**
 * Allows to override the virtually mapped object space Space
 * by the multi-level table based structure.
 *
 * This is useful for Vm or Io spaces that never run threads, and
 * saves the overhead of software page-table walks and phys-to-virt
 * translations for capability lookup.
 */
template<typename BASE>
class Obj_space_phys_override :
  public BASE,
  Obj_space_phys< Obj_space_phys_override<BASE> >
{
  typedef Obj_space_phys< Obj_space_phys_override<BASE> > Obj_space;

public:
  bool initialize()
  {
    return BASE::initialize() && Obj_space::initialize();
  }

  using BASE::ram_quota;
  static Ram_quota *ram_quota(Obj_space const *obj_sp)
  { return static_cast<Obj_space_phys_override<BASE> const *>(obj_sp)->ram_quota(); }

  bool FIASCO_FLATTEN
  v_lookup(typename Obj_space::V_pfn const &virt,
           typename Obj_space::Phys_addr *phys,
           typename Obj_space::Page_order *order,
           typename Obj_space::Attr *attribs) override
  { return Obj_space::v_lookup(virt, phys, order, attribs); }

  Page::Flags FIASCO_FLATTEN
  v_delete(typename Obj_space::V_pfn virt,
           typename Obj_space::Page_order order,
           Page::Rights rights) override
  { return Obj_space::v_delete(virt, order, rights); }

  Obj::Insert_result FIASCO_FLATTEN
  v_insert(typename Obj_space::Phys_addr phys,
           typename Obj_space::V_pfn const &virt,
           typename Obj_space::Page_order order,
           typename Obj_space::Attr page_attribs) override
  { return Obj_space::v_insert(phys, virt, order, page_attribs); }

  typename Obj_space::Capability FIASCO_FLATTEN
  lookup(Cap_index virt) override
  { return Obj_space::lookup(virt); }

  typename Obj_space::V_pfn FIASCO_FLATTEN
  obj_map_max_address() const override
  { return Obj_space::obj_map_max_address(); }

  void FIASCO_FLATTEN caps_free() override
  { Obj_space::caps_free(); }

  template<typename ...ARGS>
  Obj_space_phys_override(ARGS &&...args) : BASE(cxx::forward<ARGS>(args)...) {}

  ~Obj_space_phys_override() { caps_free(); }
};

// ------------------------------------------------------------------------
INTERFACE [!obj_space_virt]:

#include "cxx/type_traits"

/**
 * The noop version when Space already uses a multi-level array for
 * the object space.
 */
template<typename BASE>
class Obj_space_phys_override : public BASE
{
public:
  template<typename ...ARGS>
  Obj_space_phys_override(ARGS &&...args) : BASE(cxx::forward<ARGS>(args)...) {}
};

//----------------------------------------------------------------------------
IMPLEMENTATION [!obj_space_phys_avl]:

#include "config.h"
#include "kmem_alloc.h"
#include "mem.h"
#include "ram_quota.h"

PRIVATE template< typename SPACE >
inline NEEDS["mem.h"]
bool
Obj_space_phys<SPACE>::alloc_dir()
{
  static_assert(sizeof(Cap_dir) == Config::PAGE_SIZE, "cap_dir size mismatch");
  auto *dir = static_cast<Cap_dir*>(Kmem_alloc::allocator()->q_alloc(ram_quota(),
                                                         Config::page_size()));
  if (dir)
    Mem::memset_mwords(dir, 0, Config::PAGE_SIZE / sizeof(Mword));

  // Page clearing must be observable *before* the pointer to the table is
  // visible! The lookup in get_cap() happens without a lock.
  Mem::mp_wmb();

  _dir = dir;
  return _dir;
}

PRIVATE template< typename SPACE >
typename Obj_space_phys<SPACE>::Entry *
Obj_space_phys<SPACE>::get_cap(Cap_index index, bool alloc)
{
  if (auto e = get_cap(index))
    return e;

  return alloc ? caps_alloc(index) : nullptr;
}

PRIVATE template< typename SPACE >
typename Obj_space_phys<SPACE>::Entry *
Obj_space_phys<SPACE>::get_cap(Cap_index index)
{
  if (EXPECT_FALSE(!_dir))
    return nullptr;

  unsigned d_idx = cxx::int_value<Cap_index>(index) >> Obj::Caps_per_page_ld2;
  if (EXPECT_FALSE(d_idx >= Slots_per_dir))
    return nullptr;

  Cap_table *tab = _dir->d[d_idx];

  if (EXPECT_FALSE(!tab))
    return nullptr;

  unsigned offs  = cxx::get_lsb(cxx::int_value<Cap_index>(index), Obj::Caps_per_page_ld2);
  return &tab->e[offs];
}

PRIVATE template< typename SPACE >
typename Obj_space_phys<SPACE>::Entry *
Obj_space_phys<SPACE>::caps_alloc(Cap_index virt)
{
  if (EXPECT_FALSE(!_dir && !alloc_dir()))
    return nullptr;

  static_assert(sizeof(Cap_table) == Config::PAGE_SIZE, "cap table size mismatch");
  unsigned d_idx = cxx::int_value<Cap_index>(virt) >> Obj::Caps_per_page_ld2;
  if (EXPECT_FALSE(d_idx >= Slots_per_dir))
    return nullptr;

  void *mem = Kmem_alloc::allocator()->q_alloc(ram_quota(), Config::page_size());

  if (!mem)
    return nullptr;

  Obj::add_cap_page_dbg_info(mem, SPACE::get_space(this),  cxx::int_value<Cap_index>(virt));

  Mem::memset_mwords(mem, 0, Config::PAGE_SIZE / sizeof(Mword));

  // Page clearing must be observable *before* the pointer to the table is
  // visible! The lookup in get_cap() happens without a lock.
  Mem::mp_wmb();

  Cap_table *tab = _dir->d[d_idx] = static_cast<Cap_table*>(mem);
  return &tab->e[ cxx::get_lsb(cxx::int_value<Cap_index>(virt), Obj::Caps_per_page_ld2)];
}

PUBLIC template< typename SPACE >
void
Obj_space_phys<SPACE>::caps_free()
{
  if (!_dir)
    return;

  Cap_dir *d = _dir;
  _dir = nullptr;

  Kmem_alloc *a = Kmem_alloc::allocator();
  for (unsigned i = 0; i < Slots_per_dir; ++i)
    {
      if (!d->d[i])
        continue;

      Obj::remove_cap_page_dbg_info(d->d[i]);
      a->q_free(ram_quota(), Config::page_size(), d->d[i]);
    }

  a->q_free(ram_quota(), Config::page_size(), d);
}

//----------------------------------------------------------------------------
IMPLEMENTATION [obj_space_phys_avl]:

#include "lock_guard.h"

PUBLIC template< typename SPACE >
inline
Obj_space_phys<SPACE>::Obj_space_phys()
: _map(Entries::Node_allocator(ram_quota()))
{}

PRIVATE template< typename SPACE >
typename Obj_space_phys<SPACE>::Entry *
Obj_space_phys<SPACE>::get_cap(Cap_index index, bool alloc)
{
  auto guard = lock_guard(_lock);

  auto e = _map.find_node(index);
  if (e)
    return const_cast<Entry*>(&e->second);

  if (!alloc)
    return nullptr;

  auto n = _map.emplace(index);
  if (n.second == -Entries::E_nomem)
    return nullptr;

  return &n.first->second;
}

PUBLIC template< typename SPACE >
void
Obj_space_phys<SPACE>::caps_free()
{
  auto guard = lock_guard(_lock);
  _map.clear();
}

//----------------------------------------------------------------------------
IMPLEMENTATION:

#include <cassert>

//
// Utilities for map<Obj_space_phys> and unmap<Obj_space_phys>
//

IMPLEMENT template< typename SPACE >
inline NEEDS[Obj_space_phys::get_cap]
bool FIASCO_FLATTEN
Obj_space_phys<SPACE>::v_lookup(V_pfn const &virt, Phys_addr *phys,
                                Page_order *order, Attr *attribs)
{
  if (order)
    *order = Page_order(0);
  Entry *cap = get_cap(virt, false);

  if (EXPECT_FALSE(!cap))
    {
      if (order)
        *order = Page_order(Obj::Caps_per_page_ld2);
      return false;
    }

  Capability c = cap->capability();

  Obj::set_entry(virt, cap);
  if (phys)
    *phys = c.obj();
  if (c.valid() && attribs)
    *attribs = cap->rights();
  return c.valid();
}

IMPLEMENT template< typename SPACE >
inline NEEDS [Obj_space_phys::get_cap]
typename Obj_space_phys<SPACE>::Capability FIASCO_FLATTEN
Obj_space_phys<SPACE>::lookup(Cap_index virt)
{
  Entry *c = get_cap(virt, false);

  if (EXPECT_FALSE(!c))
    return Capability(0); // void

  return c->capability();
}

PUBLIC template< typename SPACE >
inline
Kobject_iface * __attribute__((nonnull))
Obj_space_phys<SPACE>::lookup_local(Cap_index virt, L4_fpage::Rights *rights)
{
  Entry *c = get_cap(virt, false);
  if (EXPECT_FALSE(!c))
    return nullptr;

  Capability cap = c->capability();
  *rights = L4_fpage::Rights(cap.rights());

  return cap.obj();
}


IMPLEMENT template< typename SPACE >
inline NEEDS[<cassert>, Obj_space_phys::get_cap]
Page::Flags FIASCO_FLATTEN
Obj_space_phys<SPACE>::v_delete(
  V_pfn virt, [[maybe_unused]] Page_order order,
  Page::Rights rights = Page::Rights::FULL())
{
  assert(order == Page_order(0));
  Entry *c = get_cap(virt, false);

  if (c && c->valid())
    {
      if (rights & L4_fpage::Rights::CR())
        c->invalidate();
      else
        c->del_rights(rights);
    }

  return Page::Flags::None();
}

IMPLEMENT template< typename SPACE >
inline
typename Obj::Insert_result FIASCO_FLATTEN
Obj_space_phys<SPACE>::v_insert(
  Phys_addr phys, V_pfn const &virt, [[maybe_unused]] Page_order order,
  Attr page_attribs)
{
  assert (order == Page_order(0));

  Entry *c = get_cap(virt, true);

  if (!c)
    return Obj::Insert_err_nomem;

  if (c->valid())
    {
      if (c->obj() == phys)
	{
	  if (EXPECT_FALSE(c->rights() == page_attribs))
	    return Obj::Insert_warn_exists;

	  c->add_rights(page_attribs);
	  return Obj::Insert_warn_attrib_upgrade;
	}
      else
	return Obj::Insert_err_exists;
    }

  Obj::set_entry(virt, c);
  c->set(phys, page_attribs);
  return Obj::Insert_ok;
}


PUBLIC template< typename SPACE >
inline
typename Obj_space_phys<SPACE>::V_pfn
Obj_space_phys<SPACE>::obj_map_max_address() const
{
  return V_pfn(Map_max_address);
}

// ------------------------------------------------------------------------------
IMPLEMENTATION [debug]:

PUBLIC template< typename SPACE >
typename Obj_space_phys<SPACE>::Entry *
Obj_space_phys<SPACE>::jdb_lookup_cap(Cap_index index)
{ return get_cap(index, false); }

// ------------------------------------------------------------------------------
IMPLEMENTATION [obj_space_virt && debug]:

PUBLIC template<typename BASE> static inline
Obj_space_phys_override<BASE> *
Obj_space_phys_override<BASE>::get_space(Obj_space *base)
{ return static_cast<Obj_space_phys_override<BASE> *>(base); }

PUBLIC template<typename BASE>
Obj::Entry *
Obj_space_phys_override<BASE>::jdb_lookup_cap(Cap_index index) override
{ return Obj_space::jdb_lookup_cap(index); }

// ------------------------------------------------------------------------------
IMPLEMENTATION [obj_space_virt && !debug]:

PUBLIC template<typename BASE> static inline
Obj_space_phys_override<BASE> *
Obj_space_phys_override<BASE>::get_space(Obj_space *)
{ return nullptr; }
